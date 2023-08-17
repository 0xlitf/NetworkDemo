cmake_minimum_required(VERSION 3.15)
project(QtNetworkWrapper CXX)

file(GLOB HEADER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB SOURCE_FILES        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")


add_library(QtNetworkWrapper ${SOURCE_FILES})
target_include_directories(QtNetworkWrapper PUBLIC include)

set_target_properties(QtNetworkWrapper PROPERTIES 
    PUBLIC_HEADER ${HEADER_SOURCE_FILES}
)

install(TARGETS QtNetworkWrapper)
