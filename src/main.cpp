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

    auto main_window = MainWindow::create();
    main_window->run();
}
