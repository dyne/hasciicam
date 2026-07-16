
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

Prerequisites:

- A C/C++ toolchain (Clang or GCC on Unix, MSVC on Windows).
- CMake 3.16+.
- Optional runtime backends discovered at configure time: SDL2 (preferred live
  display), ncurses (terminal live display), X11 (Linux only).
- macOS uses the system AVFoundation for capture; Linux uses V4L2; Windows uses
  Media Foundation with a DirectShow fallback (via vcpkg for SDL2).

Standard build (uses whichever generator CMake picks by default — Unix Makefiles
on Linux/macOS, Visual Studio on Windows):

```sh
cmake -B build .
cmake --build build -j
cmake --install build    # optional; installs to CMAKE_INSTALL_PREFIX
```

The build produces `build/hasciicam` (the CLI) and `build/libhasciicam_core.a`
(the reusable core linked by host samples under `examples/`).

Run the tests:

```sh
ctest --output-on-failure --test-dir build
```

With presets (recommended for cross-platform maintenance, requires Ninja):

```sh
cmake --list-presets
cmake --preset macos-ninja      # or linux-ninja, windows-vcpkg-ninja, wasm-emscripten
cmake --build --preset macos-ninja
```

The available presets are `linux-ninja`, `macos-ninja`, `windows-vcpkg-ninja`,
and `wasm-emscripten`. Install Ninja first (`brew install ninja`,
`apt install ninja-build`, or `choco install ninja`).

Common CMake options (all `-D<name>=ON|OFF`):

- `HASCIICAM_BUILD_CLI` — build the `hasciicam` executable (default ON).
- `HASCIICAM_ENABLE_TESTS` — build CTest targets (default ON).
- `HASCIICAM_ENABLE_SDL`, `HASCIICAM_ENABLE_X11`, `HASCIICAM_ENABLE_CURSES` —
  display backends; each is auto-disabled if its dependency is not found.
- `HASCIICAM_ENABLE_CAPTURE_V4L2` / `_MF` / `_DSHOW` / `_AVFOUNDATION` —
  capture backends per platform.
- `HASCIICAM_ENABLE_GUI` — SDL live-mode ImGui overlay (default ON).
- `HASCIICAM_ENABLE_VIRTUAL_CAMERA` — build the virtual webcam output.

Quick smoke test after building:

```sh
./build/hasciicam -h                                   # CLI help
./build/hasciicam -H                                   # CLI + AA-lib help
./build/hasciicam -d synthetic:// --frames 2 -O stdout # no-camera pipeline test
./build/hasciicam -O SDL                               # live ASCII from the default camera
```

On macOS, live mode triggers a Camera permission prompt on first run; grant it
in *System Settings → Privacy & Security → Camera* for the terminal that
launched `hasciicam`.

## On-Screen GUI (SDL Live Mode)

HasciiCam can show an optional on-screen control panel in live SDL mode.

- Build toggle: `HASCIICAM_ENABLE_GUI` (requires SDL and vendored Dear ImGui under `third_party/imgui/`)
- Activation: right mouse click in the SDL window
- Live controls: AA brightness/contrast/gamma, invert, mirror, foreground/background colors, AA font
- Camera controls: device/driver-dependent controls (when backend reports them), e.g. brightness/contrast/gamma/exposure/focus
- Pre-AA preview: small opaque grayscale preview of the luminance frame right before AA-lib rendering
- Config actions: `Save` writes TOML, `Load` reads TOML

File chooser behavior:

- Windows: native `GetOpenFileNameW` dialog
- Other platforms: fallback path field in the panel

## Startup Configuration

At startup, HasciiCam checks for `hasciicam.toml` in the current working
directory and loads it when present. Use `--config path/to/file.toml` to load a
specific TOML file instead.

Configuration precedence is:

1. Built-in defaults
2. Startup TOML (`hasciicam.toml` or `--config`)
3. Lowercase environment variables using canonical config-key names
4. Command-line options

Font selection:

- `--font list` prints all bundled AA bitmap fonts (short names)
- `--font <name>` selects one startup AA font (for example `vga16`, `vga8`, `courier`)
- `font = "vga16"` in TOML/env sets the same AA bitmap font
- `--font-face` remains HTML CSS font face (different from AA bitmap `font`)

## Runtime Pipeline (2.0)

The executable pipeline is split into explicit capture and conversion stages:

1. Parse CLI and AA-lib options.
2. Open a capture backend through `src/capture/capture_backend.c`.
3. Read frames through the backend `capture_ops` contract in `src/capture/capture.h`.
4. Convert backend pixel formats to grayscale via `src/capture/frame_convert.c`.
5. Write grayscale into AA-lib image memory and render ASCII with AA render params.
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

## Virtual Webcam Output

HasciiCam can publish the live SDL ASCII frame stream as a virtual webcam.
The published image is the ASCII render itself, not the cursor, ImGui overlay,
or window chrome.

- Windows: install the project-owned source DLL, run `hasciicam -O SDL --virtual-camera`, and use Windows 11 build 22000 or later.
- Linux: load an existing `v4l2loopback` device, then run `hasciicam -O SDL --virtual-camera --virtual-camera-device /dev/video10`.
- The feature is opt-in and only works in live SDL mode.
- Linux virtual camera size and fps are configurable; the current defaults are `1280x720` at `30` fps.
- Windows currently publishes the fixed source format `1280x720` at `30` fps.

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
