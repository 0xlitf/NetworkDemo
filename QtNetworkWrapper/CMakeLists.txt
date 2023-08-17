cmake_minimum_required(VERSION 3.15)
project(QtNetworkWrapper CXX)




add_library(QtNetworkWrapper src/QtNetworkWrapper.cpp)
target_include_directories(QtNetworkWrapper PUBLIC include)



set_target_properties(QtNetworkWrapper PROPERTIES PUBLIC_HEADER "include/QtNetworkWrapper.h")
install(TARGETS QtNetworkWrapper)
