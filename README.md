# [smtp-http-proxy: Tiny SMTP to HTTP bridge](https://el-tramo.be/smtp-http-proxy)

`smtp-http-proxy` is a lightweight daemon that posts incoming SMTP requests
to an HTTP URL as JSON.

This is useful for services that use SMTP for things like reporting, 
such as [HAProxy](http://haproxy.org)'s `email-alert`.
