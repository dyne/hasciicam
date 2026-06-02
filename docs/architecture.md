# HasciiCam 2.0 Architecture Map

This document is a short source map for maintainers. It describes the
current architecture and extension points without redefining core behavior.

## Top-Level Runtime

Main entrypoint: `src/hasciicam.c`

Runtime stages:

1. Parse AA-lib options and HasciiCam CLI options.
2. Resolve size intent (`pixels` vs `chars`) in `src/app/app_size.c`.
3. Open capture backend through `capture_open_default()`.
4. Negotiate closest supported capture size when exact input size is unavailable.
5. Derive final ASCII geometry from negotiated capture dimensions.
6. Start backend, read frames, and release frames.
7. Convert source frames to grayscale (`frame_convert.c`).
8. Write grayscale into AA-lib image memory.
9. Render ASCII with AA-lib (`aa_fastrender`).
10. Flush to live/file output through AA-lib driver selection.
11. Stop/close capture and release all buffers.

## Modules

Application shell:

- `src/hasciicam.c`: CLI, lifecycle, render loop orchestration.
- `src/app/app_size.c`: size intent planning and capture/ascii geometry helpers.

Capture ports and adapters:

- `src/capture/capture.h`: stable capture contract (`capture_ops`).
- `src/capture/capture_backend.c`: backend selection and fallback policy.
- `src/capture/capture_v4l2.c`: Linux Video4Linux2 adapter.
- `src/capture/capture_mf.c`: Windows Media Foundation adapter.
- `src/capture/capture_dshow.cpp`: Windows DirectShow fallback adapter.
- `src/capture/frame_convert.c`: frame format to grayscale conversion.

Rendering/output:

- `third_party/aalib/*`: vendored AA-lib renderer and output drivers.
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

Current host-fed capture path:

- `src/capture/capture_external.c` backend selected with device `external://`.
- Host submits frames through `hasciicam_session_submit_frame(...)`.
- Accepted frame formats are the same `capture_pixel_format` values used by
  `frame_convert.c` (for example `GRAY8`, `NV12`, `BGRA32`, `RGB24`).
- Android-facing recommendation: feed `NV12` or `BGRA32` first; add `NV21`
  explicitly only when the host camera pipeline cannot emit supported formats.
- WASM-facing recommendation: JavaScript `getUserMedia`/canvas pipelines should
  submit RGBA/BGRA or grayscale frames through the same external backend path.

Scaling decision:

- Current scaling in `frame_convert.c` stays nearest-neighbor sampling.
- This keeps CPU cost low and behavior deterministic across backends.
- Box-filter scaling remains a future optional mode if quality needs justify
  extra complexity.

Planned output adapters:

- Keep AA-lib as the canonical ASCII renderer.
- Add a memory output path for embedded hosts and deterministic tests.
- Keep AA-lib SDL (`third_party/aalib/aasdl.c`) as the active desktop SDL path for now.
  A separate `output_sdl` adapter remains deferred until a concrete mobile SDL
  lifecycle gap appears.

## Maintenance Rules

- Keep platform-specific code isolated to adapter files.
- Keep public boundaries in plain C headers.
- Allow C++ only where platform APIs require it.
- Preserve CLI and AA-lib rendering behavior unless explicitly changed.
