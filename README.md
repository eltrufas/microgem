# microgem

`microgem` is a small libevent based gemini server. It is currently unfinished
but I project it will reach a usable state within the next decade.

## Building

Using meson as a build system:

```
meson builddir
ninja -C builddir
```

## Running

Currently accepts the following commandline parameters

```
-h hostname : hostname to serve (currently ignored).
-b address  : address to listen on. 127.0.0.1 by default.
-p port     : port to listen on. 1965 by default.
-c certpath : path to tls certificate.
-k keypath  : path to tls key.
-s rootdir  : path to directory of files to serve.


```


## Current capabilities

It currently serves static files out of a single directory. No less, no more.


## Missing features

Currently we're not really doing any real parsing of URLs, so we're not
interpreting the authority section. That means microgem is not checking ports,
domain, userinfo, all that jazz. That's probably the most pressing issue.


## Contributing

I'm accepting contributions in the forms of comments, advice, code reviews
(it's my first c project), or even code patches if you're feeling super
generous at my personal email: [rafael@trfs.me](mailto:rafael@trfs.me). 

