#pragma once

#include <libtorrent/add_torrent_params.hpp>
#include <string>
#include <vector>

/// @brief Return the directory in which MicroTorrent will store all its data
///
/// ~/.microtorrent on Linux, and %USERPROFILE% on Windows
/// @return The storage directory path as a string
std::string storage_dir() noexcept;

/// @brief Resume torrents that have been saved previously
/// @return A vector of the torrents which were resumed, this may be empty
std::vector<lt::add_torrent_params> resume_torrents() noexcept;
