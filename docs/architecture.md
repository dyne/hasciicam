# HasciiCam 2.0 Architecture Map

This document is a short source map for maintainers. It describes the
current architecture and extension points without redefining core behavior.

## Top-Level Runtime

Main entrypoint: `src/hasciicam.c`

Runtime stages:

1. Parse AA-lib options and HasciiCam CLI options.
2. Open capture backend through `capture_open_default()`.
3. Start backend, read frames, and release frames.
4. Convert source frames to grayscale (`frame_convert.c`).
5. Write grayscale into AA-lib image memory.
6. Render ASCII with AA-lib (`aa_fastrender`).
7. Flush to live/file output through AA-lib driver selection.
8. Stop/close capture and release all buffers.

## Modules

Application shell:

- `src/hasciicam.c`: CLI, lifecycle, render loop orchestration.

Capture ports and adapters:

- `src/capture/capture.h`: stable capture contract (`capture_ops`).
- `src/capture/capture_backend.c`: backend selection and fallback policy.
- `src/capture/capture_v4l2.c`: Linux Video4Linux2 adapter.
- `src/capture/capture_mf.c`: Windows Media Foundation adapter.
- `src/capture/capture_dshow.cpp`: Windows DirectShow fallback adapter.
- `src/capture/frame_convert.c`: frame format to grayscale conversion.

Rendering/output:

- `src/aalib/*`: vendored AA-lib renderer and output drivers.
- Render source buffer: `aa_image(context)`.
- Render outputs: live drivers (`SDL`, `stdout`, others) and save driver
  (`save_d`) for HTML/text.

Compatibility:

- `src/compat/getopt.c`, `src/compat/getopt.h`: Windows getopt support.

## Capture Contract (Hex Port)

The capture boundary is `capture_ops` in `src/capture/capture.h`:

- `open()`
- `describe()`
- `start()`
- `read()`
- `release()`
- `stop()`
- `close()`
- `name()`

All platform camera code must stay behind this C contract.

## Backend Selection Policy

`src/capture/capture_backend.c` selects in this order:

- Windows:
  1. Media Foundation
  2. DirectShow
- Non-Windows:
  1. V4L2

This keeps fallback logic in one place.

## Build Map (CMake)

Build system of record: `CMakeLists.txt`

Targets:

- `aalib` static library: AA-lib sources plus optional drivers.
- `hasciicam` executable: main app + capture adapters + conversion.

Optional compile-time drivers:

- `SDL_DRIVER` when SDL2 is detected.
- `X11_DRIVER` on Unix when X11 is detected.
- `CURSES_DRIVER` when curses is detected.

Windows links media APIs (`mf*`) and COM support libraries.

## Platform Extension Plan

Planned capture adapters:

- macOS: AVFoundation adapter behind `capture_ops`.
- iOS: host-provided frame source first, then camera host integration.
- Android: host-provided frame source first, then camera host integration.
- WASM: JavaScript-provided frame source into the same conversion/render path.

Planned output adapters:

- Keep AA-lib as the canonical ASCII renderer.
- Add a memory output path for embedded hosts and deterministic tests.
- Keep AA-lib SDL (`src/aalib/aasdl.c`) as the active desktop SDL path for now.
  A separate `output_sdl` adapter remains deferred until a concrete mobile SDL
  lifecycle gap appears.

## Maintenance Rules

- Keep platform-specific code isolated to adapter files.
- Keep public boundaries in plain C headers.
- Allow C++ only where platform APIs require it.
- Preserve CLI and AA-lib rendering behavior unless explicitly changed.
