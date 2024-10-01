cmake_minimum_required(VERSION 3.21)

project(microtorrent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(source_files
    main.cpp)

list (TRANSFORM source_files PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)

include(FetchContent)

find_package(Slint QUIET)
if (NOT Slint_FOUND)
  message("Slint could not be located in the CMake module search path. Downloading it from Git & building it locally")
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    # `release/1` will auto-upgrade to the latest Slint >= 1.0.0 and < 2.0.0
    # `release/1.0` will auto-upgrade to the latest Slint >= 1.0.0 and < 1.1.0
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
endif (NOT Slint_FOUND)

# find_package(LibtorrentRasterbar QUIET)
# if (NOT LibtorrentRasterbar_FOUND)
#     message("libtorrent could not be located in the CMake module search path. Downloading it from Git & building it locally")
#     FetchContent_Declare(
#         LibtorrentRasterbar
#         GIT_REPOSITORY https://github.com/arvdin/libtorrent.git
#         GIT_TAG v2.0.10
#     )
# endif (NOT LibtorrentRasterbar_FOUND)

add_executable(microtorrent ${source_files})

target_link_libraries(microtorrent PRIVATE Slint::Slint)
slint_target_sources(microtorrent ui/window.slint)

# target_link_libraries(microtorrent PRIVATE LibtorrentRasterbar::torrent-rasterbar)
# On Windows, copy the Slint DLL next to the application binary so that it's found.
if (WIN32)
    add_custom_command(TARGET microtorrent POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:microtorrent> $<TARGET_FILE_DIR:microtorrent> COMMAND_EXPAND_LISTS)
endif()

