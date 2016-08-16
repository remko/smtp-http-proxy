FROM debian:jessie

RUN apt-get update -y && apt-get -y install g++ scons libcurl4-openssl-dev libboost-dev build-essential
RUN apt-get install -y libboost-system-dev libboost-program-options-dev libboost-log-dev
RUN apt-get install -y pbuilder devscripts

ADD . /opt/src
RUN cd /opt/src && \
	scons boost_libsuffix="" INSTALLDIR=packages/debian/smtp-http-proxy/usr && \
	cd packages/debian && \
	dpkg-deb --build smtp-http-proxy && \
	mkdir -p /opt/packages/debian/smtp-http-proxy && \
	mv smtp-http-proxy.deb /opt/packages/debian/smtp-http-proxy/smtp-http-proxy-0.5-1.deb && \
	cd /opt/packages && \
	dpkg-scanpackages debian/smtp-http-proxy /dev/null | gzip -9c > debian/smtp-http-proxy/Packages.gz

CMD ["/bin/true"]
