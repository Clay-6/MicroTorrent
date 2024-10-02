set shell := ["pwsh.exe", "-c"]

build:
    cmake --build build
run: build
    build/Debug/microtorrent.exe