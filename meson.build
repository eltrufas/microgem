project('microgem', 'c')

openssl = dependency('libssl')
libevent = dependency('libevent')
libevent_openssl = dependency('libevent_openssl')

executable('microgem', 'main.c', 'url.c',
  dependencies : [openssl, libevent, libevent_openssl])

