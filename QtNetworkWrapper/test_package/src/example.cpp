#include "QtNetworkWrapper.h"
#include <vector>
#include <string>

int main() {
    QtNetworkWrapper();

    std::vector<std::string> vec;
    vec.push_back("test_package");

    QtNetworkWrapper_print_vector(vec);
}
