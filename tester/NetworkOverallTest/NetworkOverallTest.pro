
QT += core testlib qml
TEMPLATE = app
NETWORK_COMPILE_MODE = SRC
include( $$PWD/../../src/Network.pri )
SOURCES += \
    $$PWD/cpp/overalltest.cpp \
    $$PWD/cpp/main.cpp
HEADERS += \
    $$PWD/cpp/overalltest.h \
    $$PWD/cpp/fusiontest1.hpp \
    $$PWD/cpp/fusiontest2.hpp \
    $$PWD/cpp/processortest1.hpp \
    $$PWD/cpp/processortest2.hpp \
