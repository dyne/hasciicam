# iOS host sample

This folder contains a minimal iOS host-shell scaffold around the public
HasciiCam C API.

## Purpose

Keep camera lifecycle and UI in the iOS app shell, while the shared core
handles conversion and ASCII rendering.

## Files

- `HasciiCamCoreBridge.h`
- `HasciiCamCoreBridge.mm`

The bridge exposes a tiny Objective-C++ wrapper that:

1. creates/destroys a `hasciicam_instance`
2. starts `external://` mode
3. accepts host camera frame buffers
4. renders and returns ASCII frame text

## Integration steps (Xcode)

1. Add these files to an iOS app target.
2. Add include path for `include/` so `<hasciicam/hasciicam.h>` resolves.
3. Link the static core library built from this repository (`hasciicam_core`).
4. Capture camera frames with AVFoundation in the app and pass buffers to
   `submitFrame`.

## Frame format

Use one of:

- `HASCIICAM_PIXFMT_NV12`
- `HASCIICAM_PIXFMT_NV21`
- `HASCIICAM_PIXFMT_BGRA32`

## Simulator/device

- Simulator: validate synthetic frame path first.
- Device: validate AVFoundation camera frames and permission flow.
