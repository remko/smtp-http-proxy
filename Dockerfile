FROM alpine:3.4

RUN apk --no-cache add g++ scons curl-dev boost-dev bash

ADD . /opt/src
RUN cd /opt/src && scons && scons check=1
RUN strip /opt/src/smtp-http-proxy

CMD ["/bin/true"]
