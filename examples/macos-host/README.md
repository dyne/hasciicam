# macOS host sample

This sample documents the host-shell pattern for macOS.

## Goal

Validate that non-CLI hosts can call the public C API in
`include/hasciicam/hasciicam.h` without depending on internal modules.

## Build

Build the main project first (macOS preset):

```sh
cmake --preset macos-ninja
cmake --build --preset macos-ninja
```

Then run the host sample (same build tree target):

```sh
cmake --build --preset macos-ninja --target hasciicam_macos_host_sample
./build/presets/macos-ninja/examples/macos-host/hasciicam_macos_host_sample
```

## Notes

- This sample uses `external://` frame submission with a synthetic grayscale
  frame.
- Camera validation on macOS is provided by the normal CLI path using
  AVFoundation (`src/capture/capture_avfoundation.mm`).
