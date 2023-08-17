#pragma once

#include <vector>
#include <string>


#ifdef _WIN32
  #define QTNETWORKWRAPPER_EXPORT __declspec(dllexport)
#else
  #define QTNETWORKWRAPPER_EXPORT
#endif

QTNETWORKWRAPPER_EXPORT void QtNetworkWrapper();
QTNETWORKWRAPPER_EXPORT void QtNetworkWrapper_print_vector(const std::vector<std::string> &strings);
