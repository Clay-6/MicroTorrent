# MicroTorrent

The universe's simplest & (possibly) smallest torrent client, written
exclusively for a school project.

Made with [Slint](https://slint.dev) for the UI and [libtorrent-rasterbar](https://libtorrent.org)
for the Bittorrent implementation.

## Usage

Download the latest release from 
[the GitHub releases page](https://github.com/Clay-6/MicroTorrent/releases/latest)
and extract the archive anywhere you like. Run the executable the normal way and,
hopefully, it should Just Work™.

If you receive a Windows Defender popup, put your faith in me to not be pwning you
and select run anyway. If you don't trust me, then you can always [build it yourself](#building)
after verifying that the source code isn't malicious.

## Building

All you need in order to build the project is cmake v3.21, it _should_ just work.
If you have the [Just](https://github.com/casey/just) command runner, then after setting
up a directory `build` for cmake to build in, `just build` will build everything and `just run`
will run the program immediately. Otherwise, you can just use cmake as normal.

Just don't try compiling from a FAT32 drive with MSVC, you'll run into an internal linker error :D
