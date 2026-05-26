# AGENTS.md - hasciicam

This repository is the C implementation of HasciiCam: a terminal program that
captures live video, converts it to luminance, renders that luminance as ASCII
through the bundled AA-lib code, and outputs it either live, as HTML, or as
plain text.

Keep the code in C. Prefer small, readable changes over rewrites. This is old
software with a clear shape: preserve its direct style, isolate portability
work behind small adapters, and avoid adding dependencies unless they are
clearly worth it.

## Build System

Use CMake as the build system of record:

```sh
mkdir build
cd build
cmake .. -G Ninja
ninja
```

On Windows with MSVC and Ninja, run the command from a Visual Studio Developer
Command Prompt/PowerShell, or initialize the environment first:

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

Without that environment, Ninja may find `cl.exe` but fail to find standard C
headers such as `stdio.h` because `INCLUDE` and `LIB` are not set.

`CMakeLists.txt` builds:

- `aalib`: a static library from `src/aalib/*.c`.
- `hasciicam`: the application in `src/hasciicam.c`, linked with `aalib`.

CMake optionally enables display backends when development packages are found:

- `SDL_DRIVER` when SDL2 is found.
- `X11_DRIVER` on non-Apple Unix when X11 is found.
- `CURSES_DRIVER` when curses is found.

Main feature toggles are explicit:

- `HASCIICAM_BUILD_CLI`
- `HASCIICAM_ENABLE_TESTS`
- `HASCIICAM_ENABLE_SDL`
- `HASCIICAM_ENABLE_X11`
- `HASCIICAM_ENABLE_CURSES`
- `HASCIICAM_ENABLE_CAPTURE_V4L2`
- `HASCIICAM_ENABLE_CAPTURE_MF`
- `HASCIICAM_ENABLE_CAPTURE_DSHOW`
- `HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION`

Cross-platform configure presets are in `CMakePresets.json`:

- `windows-vcpkg-ninja`
- `linux-ninja`
- `macos-ninja`
- `wasm-emscripten`

With vcpkg on Windows, configure with the vcpkg toolchain file when optional
packages should be discovered:

```bat
cmake -S . -B build-msvc -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-msvc
```

SDL2 is optional but is the preferred cross-platform live display backend. If it
is missing from vcpkg, install it with `vcpkg install sdl2:x64-windows`.

The root `GNUmakefile` is a legacy Linux-oriented path. It assumes Unix linker
flags such as SDL, X11, ncurses, and `libm`. Do not treat it as the
cross-platform source of truth.

## Architecture

The program is intentionally compact:

- `src/hasciicam.c` is the application: CLI parsing, configuration, video
  capture, frame conversion, AA-lib setup, rendering loop, and cleanup.
- `src/aalib/` is a vendored AA-lib-style ASCII rendering library plus display,
  keyboard, mouse, save, font, and format drivers.
- `doc/` contains the man page and historical web documentation.
- `share/` contains desktop/shareable assets.

There is no current VSA/REPR split in the C code. If new user-facing behavior is
large enough to need structure, keep one use-case per small C module and expose
plain C functions. Do not introduce a framework.

## Runtime Flow

The current path in `src/hasciicam.c` is:

1. Install `SIGINT` handling through `quitproc()`.
2. Initialize AA-lib defaults and let AA-lib parse AA options with `aa_parseoptions()`.
3. Parse HasciiCam CLI options in `config_init()`.
4. Build a `capture_request` and open capture through `capture_open_default()`:
   - Windows order: Media Foundation, then DirectShow fallback.
   - Non-Windows order: V4L2.
5. Query capture geometry/pixel format via `describe()`, then `start()`.
6. Allocate grayscale buffer and configure AA-lib hardware parameters.
7. Initialize AA-lib output:
   - `save_d` for HTML/text modes.
   - `aa_autoinit()` for live mode, after display-driver recommendations.
8. Enter the loop:
   - `read()` one frame from the active capture backend.
   - Convert frame format to grayscale with `capture_frame_to_gray_scaled()`.
   - Copy grayscale into `aa_image(ascii_context)`.
   - Render with `aa_fastrender()`.
   - Output through `aa_flush()`.
   - `release()` the capture frame.
9. On exit, `stop()` and `close()` capture backend, close AA-lib, and free buffers.

## CLI

The CLI is defined in `src/hasciicam.c` by `long_options`, `short_options`, and
the help string.

Core options:

- `-h`, `--help`: HasciiCam help.
- `-H`, `--aahelp`: HasciiCam help plus AA-lib options.
- `-v`, `--version`: version output.
- `-q`, `--quiet`: reduce output.
- `-m`, `--mode live|html|text`: output mode.
- `-d`, `--device`: capture device, defaulting to `/dev/video` or
  `/dev/video0`.
- `-i`, `--input`: capture input channel.
- `-s`, `--size WxH`: requested capture size.
- `-o`, `--aafile`: HTML/text output file.
- `-O`, `--aadriver`: preferred live AA-lib driver, such as `SDL`, `X11`,
  `curses`, or `stdout`.
- `-D`, `--daemon`: run in the background on Unix-like systems.
- `-U`, `--uid` and `-G`, `--gid`: drop privileges.

Rendering options:

- `-S`, `--font-size`: HTML font size.
- `-a`, `--font-face`: HTML font face.
- `-r`, `--refresh`: HTML refresh interval.
- `-b`, `--aabright`: AA brightness.
- `-c`, `--aacontrast`: AA contrast.
- `-g`, `--aagamma`: AA gamma.
- `-I`, `--invert`: invert rendering.
- `-B`, `--background`: HTML background color.
- `-F`, `--foreground`: HTML foreground color.

AA-lib also parses its own options before HasciiCam parses its options. When
changing CLI parsing, preserve that ordering.

## Video Capture

Capture is now behind a small C port in `src/capture/capture.h`:

- request: desired device/input/size.
- response: actual width, height, stride, and pixel format.
- operations: open, start, read/dequeue one frame, release/requeue frame, stop,
  close.

Current adapters:

- Linux: `src/capture/capture_v4l2.c`.
- Windows primary: `src/capture/capture_mf.c` (Media Foundation).
- Windows fallback: `src/capture/capture_dshow.cpp` (DirectShow).

Backend selection lives in `src/capture/capture_backend.c`. The render loop
sees only `capture_frame` and metadata and does not perform platform-specific
I/O.

## Public Embedding API

The reusable host-facing API is in `include/hasciicam/hasciicam.h`.

Core lifecycle:

- `hasciicam_create()`
- `hasciicam_start_external(...)`
- `hasciicam_submit_frame(...)`
- `hasciicam_render_frame()`
- `hasciicam_get_ascii_frame(...)`
- `hasciicam_stop()`
- `hasciicam_destroy()`

This API is implemented in `src/public/hasciicam_api.c` and linked from
`hasciicam_core`.

## Host Samples

Host samples live under `examples/`:

- `examples/macos-host`: minimal C sample using synthetic external frames.
- `examples/ios`: Objective-C++ bridge scaffold for AVFoundation app shells.
- `examples/android`: JNI bridge scaffold for Android app shells.
- `examples/wasm`: browser sample scaffold (`wasm_entry.c`, `index.html`,
  `main.js`) for Emscripten builds.

## Frame Conversion

The current frame converter assumes YUYV/YUV422 input and uses only the Y
luminance byte:

- `YUV422_to_grey()` samples by fixed byte steps.
- `YUV422_to_grey_scaled()` maps source pixels to the AA-lib image dimensions.

The AA image buffer is not RGB. It is a grayscale/luminance image consumed by
AA-lib. When adding camera backends, either request YUYV-like luminance formats
or add explicit conversion functions to this layer. Keep conversion functions
plain and testable.

## AA-lib Rendering

AA-lib is bundled in `src/aalib`.

Important public types in `aalib.h`:

- `aa_context`: active rendering/display context.
- `aa_driver`: output/display driver.
- `aa_kbddriver`: keyboard driver.
- `aa_mousedriver`: mouse driver.
- `aa_renderparams`: brightness, contrast, gamma, dither, inversion.
- `aa_format` and `aa_savedata`: file output format and destination.

Rendering path:

- The app writes luminance into `aa_image(ascii_context)`.
- `aa_fastrender()` or `aa_render()` converts luminance into `textbuffer` and
  `attrbuffer`.
- `aa_flush()` writes the rendered text through the active driver.

`aarender.c` contains the full renderer and dithering path. `aafastre.c`
contains the faster renderer used by the current capture loop.

## AA Drivers

Display drivers are selected from the compile-time `aa_drivers[]` table in
`src/aalib/aaregist.c`.

Current driver order is:

1. DOS when `DJGPP` is defined.
2. SDL when `SDL_DRIVER` is defined.
3. X11 when `X11_DRIVER` is defined.
4. Linux console when `LINUX_DRIVER` is defined.
5. curses when `CURSES_DRIVER` is defined.
6. OS/2 when `OS2_DRIVER` is defined.
7. `stdout`.
8. `stderr`.

Live mode recommends drivers in this order unless `-O` is used:

1. `SDL`
2. `X11`
3. `curses`
4. `linux`
5. `stdout`

`aa_autoinit()` first tries the recommended drivers, then falls back through
`aa_drivers[]`. For Windows and macOS, SDL is the best cross-platform live
driver to preserve. X11, Linux console, curses, and POSIX terminal behavior
should remain optional.

Keyboard drivers are registered in `aakbdreg.c`; mouse drivers are registered
in `aamoureg.c`. HasciiCam itself currently does not depend deeply on keyboard
or mouse input, so keep portability work focused on display and capture first.

## Save/Text/HTML Output

File output uses AA-lib's `save_d` driver in `src/aalib/aasave.c`.

In `HTML` mode:

- HasciiCam fills `hascii_header`.
- `hascii_format` describes the HTML wrapper, escaping rules, attributes, and
  trailer.
- Frames are written to `aafile.tmp`, then renamed to the requested output file.

In `TEXT` mode:

- `aa_text_format` is used.
- The configured output file is overwritten with the latest frame.

The save driver writes from AA-lib `textbuffer` and `attrbuffer`, so file output
and live output share the same render stage.

## Portability Notes

The portability target is Windows and macOS while preserving the C codebase.
Treat the problem as replacing platform IO, not replacing the renderer.

Known Linux/POSIX assumptions in the current code:

- V4L2 types, ioctls, and mmap streaming in `src/hasciicam.c`.
- `unistd.h`, `getopt.h`, `setuid()`, `setgid()`, `setgroups()`, `daemon()`,
  `close()`, `mmap()`, `munmap()`, and Unix signals.
- `aastdout.c` uses `ioctl(TIOCGWINSZ)` and `unistd.h`.
- CMake links `m`, which is not a separate library on MSVC.
- Some AA-lib drivers are guarded by compile definitions, but the source list
  still includes several Unix-oriented files.

The Windows build includes working camera capture with backend fallback:
Media Foundation first, DirectShow second.

Preferred direction:

- Keep AA-lib and rendering in C.
- Keep SDL as the cross-platform display path.
- Put camera capture behind a small port/adapter interface.
- Keep Linux V4L2 as one adapter.
- Add macOS capture through a narrow adapter, likely implemented with the
  platform camera API behind a C-callable boundary if needed.
- Add Windows capture through a narrow adapter, likely implemented with a
  platform camera API behind a C-callable boundary if needed.
- Add small compatibility wrappers only where they remove repeated `#ifdef`
  blocks.

Do not introduce a large abstraction layer. The goal is to let the old program
keep its shape while making the OS-specific edges replaceable.

## Development Rules

- Keep edits minimal and local.
- Prefer native C doc-comments above new functions.
- Use clear names from the domain: capture, frame, luminance, render, driver,
  save, context.
- Avoid clever algorithms. Use well-known conversion and scaling methods.
- Do not add dependencies unless asked; suggest them first when very useful.
- Preserve existing command-line behavior unless explicitly changing it.
- Update the man page and README when changing user-facing behavior.
- For portability work, compile on the target platform or document why it could
  not be compiled.

## Testing

Testing policy:

- Use CTest as the single automated test runner.
- Prefer plain C test executables and small CMake/CTest wrappers.
- Do not add a unit-test framework unless plain C + CTest becomes clearly
  insufficient.
- Keep CI deterministic: no real camera requirement and no mandatory windowed
  display requirement.
- Put generated test artifacts under the build tree, not the source tree.

Run tests with CTest:

```sh
ctest --output-on-failure --test-dir build
```

Current tests:

- `frame_convert`: deterministic conversion coverage.
- `core_link`: public embedding API link/creation smoke.
- `pipeline_smoke`: synthetic frame end-to-end render smoke via public API.

Planned test tiers are documented in `.gestalt/plans/test-coverage.org`:

- Unit/core tests: plain C executables for conversion, public API, and
  synthetic pipeline behavior.
- Deterministic CLI smoke tests: run the real `hasciicam` executable with a
  synthetic capture source and bounded frame count, then check stdout/text/HTML
  contents.
- Visual tests: opt-in CTest tests that open SDL with a synthetic source and
  verify clean exit.
- Camera tests: opt-in CTest tests that require a maintainer-provided real
  camera selector.

When implementing the planned CLI tests, add:

- `synthetic://` capture backend selected only by explicit device string.
- `--frames N` bounded run option that stops after N rendered frames.
- CTest checks for `-h`, `-H`, `-O stdout`, `-m text`, and `-m html`.

Do not let synthetic capture become a fallback for failed real camera backends;
it must be explicit so camera regressions stay visible.

For code changes:

1. Run the CMake/Ninja build.
2. Run `ctest --output-on-failure`.
3. Run `hasciicam -h`.
4. Run `hasciicam -H` when AA-lib option parsing changed.
5. On Linux with a camera, smoke-test:
   - live mode with `-O SDL` or `-O curses`.
   - text mode with `-m text -o hasciicam.asc`.
   - HTML mode with `-m html -o hasciicam.html`.
6. When changing conversion code, add a small testable helper or standalone
   check rather than relying only on live camera output.

On Windows or macOS, the minimum useful smoke test is: configure with CMake,
build with Ninja, run help output, and run a display/capture smoke test if the
platform capture adapter exists.

See `docs/smoke-tests.md` for copyable per-platform manual smoke commands.
