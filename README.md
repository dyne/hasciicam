
```
88  88    db    .dP"Y8  dP""b8 88 88  dP""b8    db    8b    d8
88  88   dPYb   `Ybo." dP   `" 88 88 dP   `"   dPYb   88b  d88
888888  dP__Yb  o.`Y8b Yb      88 88 Yb       dP__Yb  88YbdP88
88  88 dP""""Yb 8bodP'  YboodP 88 88  YboodP dP""""Yb 88 YY 88

                [ (h)ascii for the masses! ]

```

Hasciicam makes it possible to have live ascii video on the web. It
captures video from a tv card and renders it into ascii, formatting
the output into an html page with a refresh tag or in a live ascii
window or in a simple text file as well, giving the possiblity to
anybody that has a bttv card. a linux box and a cheap modem line to
show a live asciivideo feed that can be browsable without any need for
plugin, java etc. (which was an issue, back in 2001 when it was done).

# BUILD FROM SOURCE

To compile the sourcecode use CMake
```sh
cmake -B build .
cmake --build build
cmake --install build
```

With presets (recommended for cross-platform maintenance):
```sh
cmake --list-presets
cmake --preset windows-vcpkg-ninja
```
Other presets are `linux-ninja`, `macos-ninja`, and `wasm-emscripten`.

## Runtime Pipeline (2.0)

The executable pipeline is split into explicit capture and conversion stages:

1. Parse CLI and AA-lib options.
2. Open a capture backend through `src/capture/capture_backend.c`.
3. Read frames through the backend `capture_ops` contract in `src/capture/capture.h`.
4. Convert backend pixel formats to grayscale via `src/capture/frame_convert.c`.
5. Write grayscale into AA-lib image memory and render ASCII with `aa_fastrender`.
6. Flush output through AA-lib driver selection (`SDL`, `stdout`, save drivers).

This keeps platform camera code out of the render path and lets new backends
plug in without changing AA-lib behavior.

## Windows Notes

On Windows, video capture backends are tried in this order:
1. Media Foundation (new API)
2. DirectShow (old API fallback)

Live output preference remains SDL first when available.

The `-d` option on Windows is treated as a camera matcher (friendly-name substring),
not as a `/dev/video*` path.
Use `-d ""` to select the first available camera.

Quick live smoke test:
```powershell
.\build-sdl-rel\hasciicam.exe -q -d "" -O SDL
```

When a matcher does not resolve, startup logs show backend fallback attempts.

For local MSVC builds in this repo, a Release configuration is recommended:
```powershell
cmake -S . -B build-msvc-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-msvc-rel
```

# CREDITS

Hasciicam is designed, written and maintained by [Jaromil](https://jaromil.dyne.org)

People who contributed to this project:
- jan hubicka and the aalib crew - the asci rendering library
- gerd knorr - grab was inspired by his webcam sourcecode
- mathop aka josto - help on css with style
- august black - hacks for iomegabuz
- boffh - hacks for usb cams
- martin guy - karma to avoid buffer overflows
- rat - text dump
- pbm & megabug - watching ascii horizons
- rapid - security and bugfixes
- alessandro preite martinez - sgi irix support (0.9)
- thomas pfau - ftp library
- blended - wider webcam support
- dan stowell - v4l2 api support

Special thanks to:

- LOA hacklab milano for donating a pentium100mhz development box
- hell voyager for donating an hauppage bttv brooktree card which made it possible to have releases :) )
- acme + rasty + martinez for very good vibez!
- servus.at, maddler.net, flyinglinux.net, autistici.org high quality bandwidth lets people get slashdotted!
- FREAKNET medialab catania :: http://freaknet.org for knowledge, place, sun and connectivity under the vulcano!

# LICENSE

This source code is free software; you can redistribute it and/or
modify it under the terms of the GNU Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This source code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Please refer to
the GNU Public License for more details.

You should have received a copy of the GNU Public License along with
this source code; if not, write to: Free Software Foundation, Inc.,
675 Mass Ave, Cambridge, MA 02139, USA.
