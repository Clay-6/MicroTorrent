cmake_minimum_required(VERSION 3.15)

project(MICROTORRENT CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(source_files
    main.cpp)

list (TRANSFORM source_files PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)

add_executable(microtorrent ${source_files})