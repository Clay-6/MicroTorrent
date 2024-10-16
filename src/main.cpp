#include <filesystem>
#include <iostream>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>

#include "backend.hpp"
#include "slint.h"
#include "window.h"

int main() {
    if (!std::filesystem::exists(storage_dir())) {
        std::filesystem::create_directory(storage_dir());
    }
    std::cout << "Storing at " << storage_dir() << '\n';

    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::alert_mask,
                     lt::alert_category::error | lt::alert_category::storage |
                         lt::alert_category::status);
    lt::session session(settings);

    auto torrent_params = resume_torrents();
    std::vector<lt::torrent_handle> handles;
    if (torrent_params.size()) {
        handles.reserve(torrent_params.size());
        for (auto const& param : torrent_params) {
            handles.push_back(session.add_torrent(param));
        }
    }

    auto main_window = MainWindow::create();
    main_window->run();
}
