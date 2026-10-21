#include "zyphie/version.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "Zyphie " << zyphie::version() << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        std::cout << "Usage: zyph --help | --version\n"
                     "Engineering foundation: archive operations are not implemented.\n";
        return 0;
    }
    std::cerr << "Unsupported command. Use zyph --help. Archive operations are not implemented.\n";
    return 2;
}
