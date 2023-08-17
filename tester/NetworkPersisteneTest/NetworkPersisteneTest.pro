
QT += core testlib qml gui widgets
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../src/Network.pri )
SOURCES += \
    $$PWD/cpp/persistenetest.cpp \
    $$PWD/cpp/main.cpp
HEADERS += \
    $$PWD/cpp/persistenetest.h
