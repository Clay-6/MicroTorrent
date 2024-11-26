#include "backend.hpp"

#include <filesystem>
#include <fstream>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace mt {
    std::string storage_dir() noexcept {
        std::string root_dir;
        // Each platform needs its own handling to find the desired directory,
        // so do it in a preprocessor directive so only one version is compiled
#ifdef _WIN32
        root_dir = getenv("APPDATA");
        root_dir += "/microtorrent";
#elif __linux__
        root_dir = getenv("HOME") + "/.microtorrent";
        root_dir += "/.microtorrent"
#endif
        return root_dir;
    }

    namespace fs = std::filesystem;

    std::vector<char> load_file(const char *filename) {
        std::ifstream ifs(filename, std::ios_base::binary);
        ifs.unsetf(std::ios_base::skipws); // Don't skip whitespace
        return {std::istream_iterator<char>(ifs), std::istream_iterator<char>()};
    }

    std::vector<lt::add_torrent_params> resume_torrents() noexcept {
        std::string resume_dir = storage_dir() + "/resume-files";
        std::vector<lt::add_torrent_params> torrents;
        if (!fs::exists(resume_dir)) {
            fs::create_directory(resume_dir);
            return {}; // Just return an empty list since the directory didn't exist anyways
        }

        fs::directory_iterator dir(resume_dir);

        for (const fs::directory_entry &entry: dir) {
            std::ifstream ifs(entry.path(), std::ios_base::binary);
            ifs.unsetf(std::ios_base::skipws);
            // Load the file into a char buffer
            std::vector<char> buf{std::istream_iterator<char>(ifs),
                                  std::istream_iterator<char>()};
            if (!buf.empty()) {
                lt::add_torrent_params params = lt::read_resume_data(buf);
                torrents.emplace_back(params);
            }
        }

        return torrents;
    }
} // namespace mt