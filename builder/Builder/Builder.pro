
QT       += core

TEMPLATE = lib

CONFIG += staticlib

CONFIG( debug, debug | release ) {
    TARGET = Networkd
}

CONFIG( release, debug | release ) {
    TARGET = Network
}

NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../src/Network.pri )

DESTDIR = $$JQNETWORK_BIN_DIR
