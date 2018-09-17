# websockchat

# ABOUT

Simple implementation of a chat server using libsea.

You can obtain from:

https://github.com/haxworx/libsea

Support for SSL/TLS.

To ensure TLS, your certificates and keys must be valid. Also
when connecting to the server via a Javascript websocket it's
crucial to match the FQDN with that of the certificate.

Browsers do not like to serve unencrypted websockets over
HTTP. Therefore with HTTPs you must use the wss:// protocol
instead of the ws:// plain-text.

# INSTALLATION

After installing libsea:

$ make (or gmake)

$ ./websockchat

