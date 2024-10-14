#pragma once

#include <string>

/// @brief Return the directory in which MicroTorrent will store all its data
///
/// ~/.microtorrent on Linux, and %USERPROFILE% on Windows
/// @return The storage directory path as a string
std::string storage_dir();