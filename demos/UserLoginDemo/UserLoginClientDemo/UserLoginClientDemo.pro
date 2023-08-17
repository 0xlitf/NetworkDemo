
QT += core qml quick
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../../src/Network.pri )
SOURCES += \
    $$PWD/cpp/main.cpp
RESOURCES += \
    $$PWD/qml/qml.qrc
