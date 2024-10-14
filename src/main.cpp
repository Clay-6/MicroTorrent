#include <iostream>

#include "backend.hpp"
#include "slint.h"
#include "window.h"

int main() {
    std::cout << "Storing at " << storage_dir() << '\n';
    auto main_window = MainWindow::create();
    main_window->run();
}