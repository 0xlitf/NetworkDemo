
QT *= core network concurrent
CONFIG *= c++11
CONFIG *= c++14
INCLUDEPATH *= \
    $$PWD/include/
# 定义Network的版本
NETWORK_VERSIONSTRING = 0.6.6

# 准备bin库目录
NETWORK_BIN_NO1_DIR = Network$$NETWORK_VERSIONSTRING/Qt$$[QT_VERSION]
NETWORK_BIN_NO2_DIR = $$QT_ARCH
NETWORK_BIN_NO3_DIR = $$[QMAKE_XSPEC]
NETWORK_BIN_NO3_DIR ~= s/g\+\+/gcc
# 根据编译参数，追加static名称
contains( CONFIG, static ) {
    NETWORK_BIN_NO3_DIR = $$NETWORK_BIN_NO3_DIR-static
}
NETWORK_BIN_DIR = $$PWD/bin/$$NETWORK_BIN_NO1_DIR/$$NETWORK_BIN_NO2_DIR/$$NETWORK_BIN_NO3_DIR
#message($$NETWORK_BIN_DIR)
# 若bin目录不存在则创建
!exists( $$NETWORK_BIN_DIR ) {
    mkpath( $$NETWORK_BIN_DIR )
}
# 根据不同系统，选择合适的名字
unix | linux | mingw {
    CONFIG( debug, debug | release ) {
        NETWORK_LIB_FILENAME = libNetworkd.a
    }
    CONFIG( release, debug | release ) {
        NETWORK_LIB_FILENAME = libNetwork.a
    }
}
else: msvc {
    CONFIG( debug, debug | release ) {
        NETWORK_LIB_FILENAME = Networkd.lib
    }
    CONFIG( release, debug | release ) {
        NETWORK_LIB_FILENAME = Network.lib
    }
}
else {
    error( unknow platfrom )
}
# 生成bin路径
NETWORK_LIB_FILEPATH = $$NETWORK_BIN_DIR/$$NETWORK_LIB_FILENAME
# 如果未指定编译模式，并且本地存在bin文件，那么使用bin文件
!equals(NETWORK_COMPILE_MODE, SRC) {
    exists($$NETWORK_LIB_FILEPATH) {
        NETWORK_COMPILE_MODE = LIB
    }
    else {
        NETWORK_COMPILE_MODE = SRC
    }
}
equals(NETWORK_COMPILE_MODE,SRC) {
    HEADERS *= \
        $$PWD/include/foundation.h \
        $$PWD/include/foundation.inc \
        $$PWD/include/package.h \
        $$PWD/include/package.inc \
        $$PWD/include/connect.h \
        $$PWD/include/connect.inc \
        $$PWD/include/connectpool.h \
        $$PWD/include/connectpool.inc \
        $$PWD/include/server.h \
        $$PWD/include/server.inc \
        $$PWD/include/processor.h \
        $$PWD/include/processor.inc \
        $$PWD/include/client.h \
        $$PWD/include/client.inc \
        $$PWD/include/lan.h \
        $$PWD/include/lan.inc
    SOURCES *= \
        $$PWD/src/foundation.cpp \
        $$PWD/src/package.cpp \
        $$PWD/src/connect.cpp \
        $$PWD/src/connectpool.cpp \
        $$PWD/src/server.cpp \
        $$PWD/src/processor.cpp \
        $$PWD/src/client.cpp \
        $$PWD/src/lan.cpp
}
else : equals(NETWORK_COMPILE_MODE,LIB) {
    LIBS *= $$NETWORK_LIB_FILEPATH
}
else {
    error(unknow NETWORK_COMPILE_MODE: $$NETWORK_COMPILE_MODE)
}
# 如果开启了qml模块，那么引入Network的qml扩展部分
contains( QT, qml ) {
    HEADERS *= \
        $$PWD/include/clientforqml.h \
        $$PWD/include/clientforqml.inc
    SOURCES *= \
        $$PWD/src/clientforqml.cpp
    RESOURCES *= \
        $$PWD/qml/NetworkQml.qrc
    QML_IMPORT_PATH *= \
        $$PWD/qml/
}
DEFINES *= NETWORK_COMPILE_MODE_STRING=\\\"$$NETWORK_COMPILE_MODE\\\"
DEFINES *= NETWORK_VERSIONSTRING=\\\"$$NETWORK_VERSIONSTRING\\\"
