
QT += core
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../../src/Network.pri )
HEADERS += \
    $$PWD/cpp/ping.hpp
SOURCES += \
    $$PWD/cpp/main.cpp
