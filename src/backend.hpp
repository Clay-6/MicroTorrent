#pragma once

#include <string>

/// @brief Return the directory in which MicroTorrent will store all its data
/// @return The storage directory path as a string
std::string storage_dir() {
    std::string root_dir;
#ifdef _WIN32
    root_dir = getenv("APPDATA");
#elif __linux__
    root_dir = getenv("HOME");
#endif
    return root_dir + "/microtorrent";
}