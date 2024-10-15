#include "backend.hpp"

#include <filesystem>
#include <fstream>
#include <libtorrent/read_resume_data.hpp>

std::string storage_dir() noexcept {
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

std::vector<lt::add_torrent_params> resume_torrents() noexcept {
    std::vector<lt::add_torrent_params> torrents;
    std::filesystem::directory_iterator dir(storage_dir() + "/resume-files");
    for (const std::filesystem::directory_entry& entry : dir) {
        std::ifstream ifs(entry.path(), std::ios_base::binary);
        ifs.unsetf(std::ios_base::skipws);
        std::vector<char> buf{std::istream_iterator<char>(ifs),
                              std::istream_iterator<char>()};
        if (buf.size() > 0) {
            lt::add_torrent_params params = lt::read_resume_data(buf);
            torrents.emplace_back(params);
        }
    }

    return torrents;
}