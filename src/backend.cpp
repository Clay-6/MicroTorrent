#include "backend.hpp"

#include <filesystem>
#include <fstream>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <vector>
#include <iostream>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/load_torrent.hpp>

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
        if (!fs::exists(filename)) return {};

        std::ifstream ifs(filename, std::ios_base::binary);
        ifs.unsetf(std::ios_base::skipws); // Don't skip whitespace
        return {std::istream_iterator<char>(ifs), std::istream_iterator<char>()};
    }

    lt::add_torrent_params load_torrent(const std::string &torrent) {
        // all magnet links must start with `magnet:?`, so we can use that
        // to differentiate between a magnet & file torrent
        if (torrent.starts_with("magnet:?")) {
            return lt::parse_magnet_uri(torrent);
        } else {
            return lt::load_torrent_file(torrent);
        }
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

    void save_torrent_data(const lt::save_resume_data_alert *alert) {
        /*
         * Yes, I know the name method is deprecated but the filename has to be both unique and
         * easy to generate. The torrent name fits the second perfectly and _mostly_ fits the first.
         * infohash would be better but that can't be a filename unfortunately
         * */
        std::string path = storage_dir() + "/resume-files/" + alert->handle.name() + ".resume_file";
        std::ofstream of(path, std::ios_base::binary);
        of.unsetf(std::ios_base::skipws);
        const std::vector<char> b = lt::write_resume_data_buf(alert->params);
        of.write(b.data(), int(b.size()));
    }

    const char *state(lt::torrent_status::state_t s) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
        switch (s) {
            case lt::torrent_status::checking_files:
                return "checking";
            case lt::torrent_status::downloading_metadata:
                return "dl metadata";
            case lt::torrent_status::downloading:
                return "downloading";
            case lt::torrent_status::finished:
                return "finished";
            case lt::torrent_status::seeding:
                return "seeding";
            case lt::torrent_status::checking_resume_data:
                return "checking resume";
            default:
                return "<>";
        }

    }

#ifdef __clang__
#pragma clang diagnostic pop
#endif
} // namespace mt
