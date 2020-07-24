project('gus', 'c')

openssl = dependency('libssl')
libevent = dependency('libevent')
libevent_openssl = dependency('libevent_openssl')

executable('gus', 'main.c',
  dependencies : [openssl, libevent, libevent_openssl])

