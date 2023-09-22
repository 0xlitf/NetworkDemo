cmake_minimum_required(VERSION 3.24)
project(NetworkWrapper LANGUAGES CXX)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(NETWORK_COMPILE_MODE_STRING "lib")

set(QT6_DIR C:/Qt/6.5.2/msvc2019_64)
SET(CMAKE_PREFIX_PATH ${QT6_DIR}/lib/cmake)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Core5Compat Concurrent Qml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Core5Compat Concurrent Qml)

qt_standard_project_setup(REQUIRES 6.5)

set(QT_DEBUG_FIND_PACKAGE ON)

file(GLOB HEADER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB SOURCE_FILES        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(NetworkWrapper SHARED
#    ${HEADER_SOURCE_FILES}
#    ${SOURCE_FILES}
# include/clientforqml.h
include/foundation.h
include/client.h
include/connect.h
include/connectpool.h
include/lan.h
include/network.h
include/package.h
include/processor.h
include/server.h
)

target_link_libraries(NetworkWrapper
    PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Core5Compat
    Qt6::Concurrent
    Qt6::Qml
)

install(TARGETS NetworkWrapper
    LIBRARY DESTINATION lib 
    ARCHIVE DESTINATION lib 
    RUNTIME DESTINATION bin 
    PUBLIC_HEADER DESTINATION include
)

#set(CMAKE_INSTALL_PREFIX "C:/0xlitf/Network/install")
install(DIRECTORY include/${PROJECT_NAME}/
    DESTINATION "install"
    FILES_MATCHING 
    PATTERN "*.h"          # 匹配.h文件
    PATTERN "*.hpp"        # 匹配.hpp文件
    PATTERN ".svn" EXCLUDE # 排除.svn文件
)
