#include <iostream>

#include "slint.h"
#include "utils.hpp"
#include "window.h"

int main() {
    std::cout << "Storing at " << storage_dir() << '\n';
    auto main_window = MainWindow::create();
    main_window->run();
}