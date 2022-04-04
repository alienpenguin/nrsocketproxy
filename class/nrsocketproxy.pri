NRPROXY_VERSION=0.0.1

#----- DO NOT MODIFY BELOW HERE -----
HEADERS += $$PWD/nrsocketproxy.h
SOURCES += $$PWD/nrsocketproxy.cpp

SSLSRVPATH = $$PWD/ext/ssl/src/ssl
INCLUDEPATH += $$PWD

include($$SSLSRVPATH/sslserver.pri)
