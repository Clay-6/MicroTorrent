#pragma once

#include <libtorrent/add_torrent_params.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mt {
/// @brief Return the directory in which MicroTorrent will store all its data
///
/// $HOME/.microtorrent on Linux, and %USERPROFILE% on Windows
/// @return The storage directory path as a string
    std::string storage_dir() noexcept;

/// @brief Resume torrents that have been saved previously
/// @return A vector of the torrents which were resumed. This may be empty
/// if no torrents were actually resumed
    std::vector<lt::add_torrent_params> resume_torrents() noexcept;

/// @brief Load a file from disk
/// @return A vector of the bytes making up the requested file. This
/// will be empty if the file is not found
    std::vector<char> load_file(const char *path);
} // namespace mt