#pragma once

#include <libtorrent/add_torrent_params.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>

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

    /// @brief return the name of a torrent status enum
    /// @return The string form for the state provided
    char const *state(lt::torrent_status::state_t s);

    /// @brief Write the resume data provided to disk for storage
    void save_torrent_data(const lt::save_resume_data_alert *alert);

    /// @brief Load a torrent file from either a file path or magnet link
    /// @returns The `add_torrent_params` object for the requested torrent.
    /// Will throw an exception if parsing fails
    lt::add_torrent_params load_torrent(const std::string &torrent);

    /// @brief Create a torrent file for a folder & save it to the provided path
    void create_torrent(const std::string &folder, const std::string_view &save_path, const std::string &tracker_url);

    /// @brief Sanitise the given path for use with libtorrent functions, transforming it in-place
    ///
    /// Removes double (`"`) & single (`'`) quotes from the ends
    std::string sanitise_path(const std::string_view &path);

    struct add_request {
        std::string uri;
        std::string save_path;
    };

    struct remove_request {
        int id;
    };

    struct create_request {
        std::string folder;
        std::string save_path;
        std::string tracker_url;
    };

    enum class BlacklistUpdate {
        Add,
        Remove,
    };

    struct update_blacklist_request {
        std::string ip;
        BlacklistUpdate action;
    };
} // namespace mt