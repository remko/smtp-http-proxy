FROM haproxy:1.6-alpine

# Install dependencies
RUN apk --no-cache add s6 curl boost boost-program_options boost-system
RUN \
  curl http://cdn.el-tramo.be/alpine/el-tramo.be.rsa.pub > /etc/apk/keys/el-tramo.be.rsa.pub && \
  echo http://cdn.el-tramo.be/alpine/smtp-http-proxy >> /etc/apk/repositories && \
  apk --allow-untrusted --no-cache add smtp-http-proxy

# Install configuration
ADD services /etc/s6/services
ADD haproxy.cfg /usr/local/etc/haproxy/

EXPOSE 80
WORKDIR /etc/s6/services

ENV SMTP_HTTP_URL=https://uu71rcz28i.execute-api.eu-central-1.amazonaws.com/prod/smtp2slack
ENV AWS_LAMBDA_API_KEY=YOURKEY

CMD ["s6-svscan"]
