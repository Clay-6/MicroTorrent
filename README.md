# MicroTorrent

The universe's simplest & (possibly) smallest torrent client

Made with [Slint](https://slint.dev) for the UI and [libtorrent-rasterbar](https://libtorrent.org)
for the Bittorrent implementation.

## Building

All you need in order to build the project is cmake v3.21, it _should_ just work.
If you have the [Just](https://github.com/casey/just) command runner, then after setting
up a directory `build` for cmake to build in, `just build` will build everything and `just run`
will run the program immediately. Otherwise, you can just use cmake as normal.

Just don't try compiling from a FAT32 drive with MSVC, you'll run into an internal linker error :D
