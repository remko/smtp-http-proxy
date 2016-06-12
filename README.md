# [smtp-http-proxy: Tiny SMTP to HTTP bridge](https://el-tramo.be/smtp-http-proxy)

`smtp-http-proxy` is a lightweight daemon that posts incoming SMTP requests
to an HTTP URL as JSON.

This is useful for services that use SMTP for things like reporting, 
such as [HAProxy](http://haproxy.org)'s `email-alert`.


## Installing

### Alpine

    echo http://cdn.el-tramo.be/alpine/smtp-http-proxy >> /etc/apk/repositories
    apk --allow-untrusted --no-cache add smtp-http-proxy

### Debian Stable (Jessie)

    echo 'deb http://cdn.el-tramo.be debian/smtp-http-proxy/' >> /etc/apt/sources.list
    apt-get update
    apt-get install smtp-http-proxy

## Building

    scons


## Usage

The following will start listening for SMTP connections on port 25 of all interfaces,
and send incoming messages to `https://example.com/receive-mail`

    smtp-http-proxy --bind 0.0.0.0 --port 25 --url https://example.com/receive-mail

The given URL will get a HTTP `POST` request with an `application/json` body,
of the following form:

    {
      "envelope": {
        "from": "<sender@example.com>",
        "to": [
          "<receiver1@example.com>",
          "<receiver2@example.com>"
        ]
      },
      "data": "From: sender@example.com\nDate: Sun, 12 Jun 2016 18:03:51 +0200\nSubject: Message\n\nThis is a message"
    }
