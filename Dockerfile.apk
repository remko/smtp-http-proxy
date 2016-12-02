FROM alpine:3.4

RUN apk --no-cache add g++ scons curl-dev boost-dev bash
RUN apk --no-cache add abuild
RUN apk --no-cache add build-base
ADD . /opt/src
RUN \
	echo 'PACKAGER_PRIVKEY="/opt/src/packages/alpine/build.rsa"' >> /etc/abuild.conf && \
	adduser -S build && \
	chown -R build /opt/src /var/cache/distfiles
RUN \
	sudo -u build /bin/sh -c 'cd /opt/src/packages/alpine/smtp-http-proxy && abuild'

WORKDIR /opt/src
CMD ["/bin/bash"]
