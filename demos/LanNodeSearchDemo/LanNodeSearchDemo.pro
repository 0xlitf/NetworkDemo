
QT += core testlib
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../src/Network.pri )
HEADERS +=
SOURCES += \
    $$PWD/cpp/main.cpp
