#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <curl/curl.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options.hpp>
#include <boost/optional/optional.hpp>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <json.hpp>
#include <deque>

using boost::asio::ip::tcp;
using boost::asio::ip::address;
namespace po = boost::program_options;

enum LogLevel {
	Silent = 0,
	Normal = 1,
	Verbose = 2,
	Debug = 3
};
LogLevel logLevel;

class Sender {
	public:
		virtual void send(const std::string& response, bool close = false) = 0;
};

class SMTPMessage {
	public:
		SMTPMessage(
				const std::string& from,
				const std::vector<std::string>& to,
				const std::string& data) :
					from(from),
					to(to),
					data(data) {
		}

		const std::string& getFrom() const {
			return from;
		}

		const std::vector<std::string>& getTo() const {
			return to;
		}

		const std::string& getData() const {
			return data;
		}

	private:
		std::string from;
		std::vector<std::string> to;
		std::string data;
};

class SMTPHandler {
	public:
		virtual void handle(const SMTPMessage& message) = 0;
};

using json = nlohmann::json;

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void*) {
	size_t realsize = size * nmemb;
	if (logLevel >= Debug) {
		std::cerr << std::string((const char*) contents, realsize) << std::endl;
	}
	return realsize;
}

class HTTPPoster : public SMTPHandler {
	public:
		HTTPPoster(const std::string& url) : 
				url(url),
				stopRequested(false) {
			thread = new std::thread(std::bind(&HTTPPoster::run, this));
		}

		virtual void handle(const SMTPMessage& message) override {
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				queue.push_back(message);
			}
			queueNonEmpty.notify_one();
		}

		void stop() {
			stopRequested = true;
			queueNonEmpty.notify_one();
			thread->join();
			delete thread;
			thread = 0;
		}

	private:
		void run() {
			while (!stopRequested) {
				boost::optional<SMTPMessage> message;
				{
					std::unique_lock<std::mutex> lock(queueMutex);
					queueNonEmpty.wait(lock, [this]() { return !queue.empty() || stopRequested; });
					if (stopRequested) { break; }
					message = queue.front();
					queue.pop_front();
				}
				doHandle(*message);
			}
		}

		void doHandle(const SMTPMessage& message) {
			json j;
			j["envelope"] = {
				{"from", message.getFrom()},
				{"to", message.getTo()}
			};
			j["data"] = message.getData();
			std::string body = j.dump();

			if (logLevel >= Debug) { std::cerr << "Handling: " << body << " (" << body.size() << " bytes)" << std::endl; }

			std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
			struct curl_slist *slist = NULL; 
			slist = curl_slist_append(slist, "Content-Type: application/json"); 
			curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist); 
			curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "POST");
			curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
			curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curlWriteCallback);
			curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5);
			curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
			if (logLevel >= Debug) { curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1); }

			if (logLevel >= Debug) { std::cerr << "---- Start HTTP request ----" << std::endl; }
			auto ret = curl_easy_perform(curl.get());
			if (ret != CURLE_OK) {
				std::cerr << "ERROR " << curl_easy_strerror(ret) << std::endl;
			}
			long statusCode;
			ret = curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &statusCode);
			if (ret != CURLE_OK) {
				std::cerr << "ERROR " << curl_easy_strerror(ret) << std::endl;
			}
			if (statusCode != 200) {
				std::cerr << "Error: Unexpected status code: " << statusCode << std::endl;
			}
			if (logLevel >= Debug) { std::cerr << std::endl << "---- End HTTP request ----" << std::endl; }
		}

	private:
		std::string url;
		std::atomic_bool stopRequested;
		std::thread* thread;
		std::deque<SMTPMessage> queue;
		std::mutex queueMutex;
		std::condition_variable queueNonEmpty;
};

class SMTPSession {
	public:
		SMTPSession(Sender& sender, SMTPHandler& handler) : sender(sender), handler(handler), receivingData(false) {
		}

		void reset() {
			from = boost::optional<std::string>();
			to.clear();
			dataLines = std::stringstream();
		}

		void start() {
			send("220 " + boost::asio::ip::host_name());
		}

		void receive(const std::string& data) {
			if (logLevel >= Debug) { std::cerr << "<- " << data << std::endl; }
			if (receivingData) {
				if (data == ".") {
					receivingData = false;
					send("250 Ok");
					if (from) {
						handler.handle(SMTPMessage(*from, to, dataLines.str()));
					}
					else {
						std::cerr << "Didn't receive FROM; not handling mail" << std::endl;
					}
					reset();
				}
				else {
					dataLines << data << std::endl;
				}
			}
			else {
				if (boost::algorithm::starts_with(data, "HELO") || boost::algorithm::starts_with(data, "EHLO")) {
					send("250 Hello");
				}
				else if (boost::algorithm::starts_with(data, "DATA")) {
					receivingData = true;
					send("354 Send data");
				}
				else if (boost::algorithm::starts_with(data, "QUIT")) {
					send("221 Bye", true);
				}
				else if (boost::algorithm::starts_with(data, "MAIL FROM:")) {
					reset();
					from = data.substr(10);
					send("250 Ok");
				}
				else if (boost::algorithm::starts_with(data, "RCPT TO:")) {
					to.push_back(data.substr(8));
					send("250 Ok");
				}
				else if (boost::algorithm::starts_with(data, "NOOP")) {
					send("250 Ok");
				}
				else if (boost::algorithm::starts_with(data, "RSET")) {
					reset();
					send("250 Ok");
				}
				else {
					send("502 Command not implemented");
				}
			}
		}

		void send(const std::string& data, bool closeAfterNextWrite = false) {
			if (logLevel >= Debug) { std::cerr << "-> " << data << std::endl; }
			sender.send(data, closeAfterNextWrite);
		}

	
	private:
		Sender& sender;
		SMTPHandler& handler;
		bool receivingData;
		boost::optional<std::string> from;
		std::vector<std::string> to;
		std::stringstream dataLines;
};

template<typename T>
class LineBufferingReceiver {
	public:
		LineBufferingReceiver(T& target) : target(target) {}

		template <typename InputIterator>
		void receive(InputIterator begin, InputIterator end) {
			while (begin != end) {
				char c = *begin++;
				if (c == '\n') {
					if (!buffer.empty()) {
						if (buffer.back() == '\r') {
							buffer.pop_back();
						}
						target.receive(std::string(&buffer[0], buffer.size()));
						buffer.clear();
					}
				}
				else {
					buffer.push_back(c);
				}
			}
		}

	private:
		T& target;
		std::vector<char> buffer;
};

class Session : public std::enable_shared_from_this<Session>, public Sender {
	public:
		Session(tcp::socket socket, HTTPPoster& httpPoster) :
				socket(std::move(socket)),
				smtpSession(*this, httpPoster),
				receiver(smtpSession) {
		}

		void start() {
			smtpSession.start();
		}

	private:
		void doRead() {
			auto self(shared_from_this());
			socket.async_read_some(
					boost::asio::buffer(buffer),
					[this, self](boost::system::error_code ec, std::size_t length) {
						if (!ec) {
							receiver.receive(buffer.data(), buffer.data() + length);
							doRead();
						}
					});
		}

		virtual void send(const std::string& command, bool closeAfterNextWrite) override {
			std::string cmd = command + "\r\n";
			auto self(shared_from_this());
			boost::asio::async_write(
					socket, boost::asio::buffer(cmd, cmd.size()),
					[this, self, closeAfterNextWrite](boost::system::error_code ec, std::size_t) {
						if (!ec) {
							if (closeAfterNextWrite) {
								boost::system::error_code errorCode;
								socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, errorCode);
								socket.close();
							}
							else {
								doRead();
							}
						}
					});
		}

		tcp::socket socket;
		enum { maxLength = 8192 };
		std::array<char, 8192> buffer;
		SMTPSession smtpSession;
		LineBufferingReceiver<SMTPSession> receiver;
};

class Server {
	public:
		Server(
				boost::asio::io_service& ioService, 
				boost::asio::ip::address& bindAddress,
				int port, 
				boost::optional<int> notifyFD,
				HTTPPoster& httpPoster) :
					httpPoster(httpPoster),
					acceptor(ioService, tcp::endpoint(bindAddress, port)),
					socket(ioService) {
			doAccept();
			if (notifyFD) {
				auto writeResult = write(*notifyFD, "\n", 1);
				if (writeResult <= 0) { std::cerr << "Error " << writeResult << " writing to descriptor " << *notifyFD << std::endl; }
				auto closeResult = close(*notifyFD);
				if (closeResult < 0) { std::cerr << "Error " << closeResult << " closing descriptor " << *notifyFD << std::endl; }
			}
			if (logLevel >= Normal) { std::cerr << "Listening for SMTP connections on " << tcp::endpoint(bindAddress, port) << std::endl; }
		}

	private:
		void doAccept() {
			acceptor.async_accept(socket, [this](boost::system::error_code ec) {
				if (!ec) {
					std::make_shared<Session>(std::move(socket), httpPoster)->start();
				}
				doAccept();
			});
		}

		HTTPPoster& httpPoster;
		tcp::acceptor acceptor;
		tcp::socket socket;
};

int main(int argc, char* argv[]) {
	curl_global_init(CURL_GLOBAL_ALL);

	try {
		po::options_description options("Allowed options");
		options.add_options()
			("help", "Show this help message")
			("debug", "Enable debug output")
			("notify-fd", po::value<int>(), "Write to file descriptor when ready")
			("bind", po::value<std::string>(), "SMTP address to bind (default: 0.0.0.0)")
			("port", po::value<int>(), "SMTP port to bind (default: 25)")
			("url", po::value<std::string>(), "HTTP URL");
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, options), vm);
		po::notify(vm);    

		boost::optional<int> port;
		boost::optional<int> notifyFD;
		address bindAddress;
		std::string httpURL;

		if (vm.count("help")) {
			std::cout << options << "\n";
				return 0;
		}
		if (vm.count("port")) {
			port = vm["port"].as<int>();
		}
		if (vm.count("url")) {
			httpURL = vm["url"].as<std::string>();
		}
		else {
			std::cerr << "Missing --url parameter" << std::endl;
			return -1;
		}
		if (vm.count("bind")) {
			bindAddress = address::from_string(vm["bind"].as<std::string>());
		}
		if (vm.count("debug")) {
			logLevel = Debug;
		}
		if (vm.count("notify-fd")) {
			notifyFD = vm["notify-fd"].as<int>();
		}

		boost::asio::io_service io_service;

		HTTPPoster httpPoster(httpURL);
		Server s(
				io_service, 
				bindAddress,
				boost::get_optional_value_or(port, 25),
				notifyFD,
				httpPoster
		);
		io_service.run();

		httpPoster.stop();

	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}
}
