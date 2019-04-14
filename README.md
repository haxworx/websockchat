## WEBSOCKCHAT

### ABOUT

Simple implementation of a chat server using libsea.

You can obtain from:

https://github.com/haxworx/libsea

To ensure TLS, your certificates and keys must be valid. Also
when connecting to the server via a Javascript websocket it's
crucial to match the FQDN with that of the certificate.

Browsers do not like to serve unencrypted websockets over
HTTP. Therefore with HTTPs you must use the wss:// protocol
instead of the ws:// plain-text.


### ADDING USERS

./auth <username> <password>

### INSTALLATION

After installing libsea:

$ make (or gmake)

$ ./websockchat

### WEB CLIENT (websockchat.html)

IRC-like protocol. Must authenticate before communication is
possible.

E.g.

/NICK <username>
/PASS <password>
