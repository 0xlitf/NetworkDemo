
QT += core testlib qml
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../src/Network.pri )
SOURCES += \
    $$PWD/cpp/main.cpp \
    $$PWD/cpp/benchmark.cpp
HEADERS += \
    $$PWD/cpp/benchmark.h
