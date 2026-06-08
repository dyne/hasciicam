# Testing Strategy

HasciiCam tests should stay simple, deterministic, and C-first.

## Policy

- Use CTest as the single automated test runner.
- Prefer plain C test executables and small CMake/CTest wrappers.
- Do not add a unit-test framework unless plain C + CTest becomes clearly
  insufficient.
- Keep CI deterministic: no real camera requirement and no mandatory windowed
  display requirement.
- Put generated test artifacts under the build tree, not the source tree.

## Test Tiers

### Unit and core tests

Use plain C executables for conversion, public API, and synthetic pipeline
behavior.

Current examples:

- `frame_convert`: deterministic conversion coverage.
- `core_link`: public embedding API link/creation smoke.
- `pipeline_smoke`: synthetic frame end-to-end render smoke via public API.
- `virtual_camera`: platform-neutral virtual-camera contract coverage.
- `app_virtual_camera`: SDL callback and publish-gate coverage.
- `virtual_camera_windows`, `virtual_camera_install`: Windows COM/source and install helpers.
- `virtual_camera_v4l2`: Linux V4L2 output helper coverage.

### Deterministic CLI smoke tests

These should run the real `hasciicam` executable without camera hardware.

Add the missing support first:

- `synthetic://` capture backend selected only by explicit device string.
- `--frames N` bounded run option that stops after N rendered frames.

Then add CTest checks for:

- `hasciicam -h`
- `hasciicam -H`
- `hasciicam -d synthetic:// --frames 2 -O stdout`
- `hasciicam -d synthetic:// --frames 2 -m text -o <build-output>.txt`
- `hasciicam -d synthetic:// --frames 2 -m html -o <build-output>.html`

HTML tests should assert:

- output file exists
- file is non-empty
- refresh meta tag is present
- `<PRE>` is present
- generated ASCII payload is not all whitespace

Text/stdout tests should assert non-empty, non-whitespace ASCII.

### Visual SDL tests

SDL tests should be opt-in because CI and headless machines often lack a usable
display.

Recommended CMake option:

```cmake
HASCIICAM_ENABLE_VISUAL_TESTS=OFF
```

When enabled, run:

```sh
hasciicam -d synthetic:// --frames 2 -O SDL
```

The first useful assertion is clean process exit. Pixel/screenshot assertions
can be added later only if they are reliable on the target machine.

### Real camera tests

Real camera tests should be opt-in and never part of default CI.

Recommended CMake options:

```cmake
HASCIICAM_ENABLE_CAMERA_TESTS=OFF
HASCIICAM_TEST_CAMERA_DEVICE=<device-or-matcher>
```

When enabled, run a bounded smoke test:

```sh
hasciicam -d <device-or-matcher> --frames 2 -m text -o <build-output>.txt
```

Use stdout or text output first. SDL can be covered separately by visual tests.

## Implemented Baseline

Already in repository:

1. Synthetic capture backend selected explicitly by `-d synthetic://`.
2. Bounded run option `--frames N` (stops after rendered frames).
3. Shared CLI smoke script at `tests/cli_smoke.cmake`.
4. CTest labels: `unit`, `core`, `cli`, `visual`, `camera`.
5. CLI tests: `cli_help`, `cli_aahelp`, `cli_stdout`, `cli_text`, `cli_html`.
6. Opt-in tests: `visual_sdl`, `camera_text`.
7. Virtual camera feature tests are split by platform and stay deterministic unless a real consumer is explicitly available.

Do not let synthetic capture become fallback behavior. It must be selected
explicitly so real capture regressions remain visible.
