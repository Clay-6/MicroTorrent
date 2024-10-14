#include "backend.hpp"

std::string storage_dir() {
    std::string root_dir;
#ifdef _WIN32
    root_dir = getenv("APPDATA");
#elif __linux__
    root_dir = getenv("HOME");
#endif
    return root_dir + "/microtorrent";
}