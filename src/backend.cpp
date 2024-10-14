#include "backend.hpp"

std::string storage_dir() {
    std::string root_dir;
#ifdef _WIN32
    root_dir = getenv("APPDATA");
    root_dir += "/microtorrent";
#elif __linux__
    root_dir = getenv("HOME") + "/.microtorrent";
    root_dir += "/.microtorrent"
#endif
    return root_dir;
}