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
#include <json.hpp>

using boost::asio::ip::tcp;
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

class SMTPHandler {
	public:
		virtual void handle(
				const std::vector<std::string>& from,
				const std::vector<std::string>& to,
				const std::string& data) = 0;
};

using json = nlohmann::json;

class HTTPPoster : public SMTPHandler {
	public:
		HTTPPoster(const std::string& url) : url(url) {
		}

		virtual void handle(
				const std::vector<std::string>& from,
				const std::vector<std::string>& to,
				const std::string& data) override {
			json j;
			j["envelope"] = {
				{"from", from},
				{"to", to}
			};
			j["data"] = data;
			std::string body = j.dump();

			if (logLevel >= Debug) { std::cerr << "Handling: " << body << " (" << body.size() << " bytes)" << std::endl; }

			std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
			struct curl_slist *slist = NULL; 
			slist = curl_slist_append(slist, "Content-Type: application/json"); 
			curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist); 
			curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "POST");
			curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
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
};

class SMTPSession {
	public:
		SMTPSession(Sender& sender, SMTPHandler& handler) : sender(sender), handler(handler), receivingData(false) {
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
					handler.handle(from, to, dataLines.str());
					send("221 Bye", true);
				}
				else if (boost::algorithm::starts_with(data, "MAIL FROM:")) {
					from.push_back(data.substr(10));
					send("250 Ok");
				}
				else if (boost::algorithm::starts_with(data, "RCPT TO:")) {
					to.push_back(data.substr(8));
					send("250 Ok");
				}
				else {
					send("250 Ok");
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
		std::vector<std::string> from;
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
		Session(tcp::socket socket, const std::string& httpURL) : 
				socket(std::move(socket)),
				httpPoster(httpURL),
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
		HTTPPoster httpPoster;
		SMTPSession smtpSession;
		LineBufferingReceiver<SMTPSession> receiver;
};

class Server {
	public:
		Server(
				boost::asio::io_service& ioService, 
				int port, 
				boost::optional<int> notifyFD,
				const std::string& httpURL) : 
					httpURL(httpURL),
					acceptor(ioService, tcp::endpoint(tcp::v4(), port)),
					socket(ioService) {
			doAccept();
			if (notifyFD) {
				auto writeResult = write(*notifyFD, "\n", 1);
				if (writeResult <= 0) { std::cerr << "Error " << writeResult << " writing to descriptor " << *notifyFD << std::endl; }
				auto closeResult = close(*notifyFD);
				if (closeResult < 0) { std::cerr << "Error " << closeResult << " closing descriptor " << *notifyFD << std::endl; }
			}
		}

	private:
		void doAccept() {
			acceptor.async_accept(socket, [this](boost::system::error_code ec) {
				if (!ec) {
					std::make_shared<Session>(std::move(socket), httpURL)->start();
				}
				doAccept();
			});
		}

		std::string httpURL;
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
			("port", po::value<int>(), "SMTP port")
			("url", po::value<std::string>(), "HTTP URL");
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, options), vm);
		po::notify(vm);    

		boost::optional<int> port;
		boost::optional<int> notifyFD;
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
		if (vm.count("debug")) {
			logLevel = Debug;
		}
		if (vm.count("notify-fd")) {
			notifyFD = vm["notify-fd"].as<int>();
		}

		boost::asio::io_service io_service;
		Server s(
				io_service, 
				boost::get_optional_value_or(port, 25),
				notifyFD,
				httpURL
		);
		io_service.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}
}
