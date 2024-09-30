#pragma once

#include <string>

std::string storage_dir() {
    std::string root;
#ifdef _WIN32
    root = getenv("APPDATA");
#elif __linux__
    root = getenv("HOME");
#endif
    return root + "/microtorrent";
}