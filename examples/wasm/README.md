# WASM browser camera sample

This sample renders live camera luminance as ASCII in a visible browser canvas.
JavaScript owns the one `requestAnimationFrame` loop and transfers RGBA pixels
from a hidden source canvas to the shared C core. The default **Canvas 2D** path
gets AA-lib's rendered character buffer from C and draws it with the browser's
native 2D canvas API, without asking SDL to create a window or rendering
context.

The **Rendering system** menu also exposes all available paths explicitly:

- **Canvas 2D (recommended)**: native browser drawing; no SDL canvas setup.
- **Automatic fallback**: SDL/WebGL, then SDL software, then native Canvas 2D.
- **SDL / WebGL**: SDL's accelerated GLES2/WebGL renderer only.
- **SDL software**: SDL's browser 2D-canvas renderer only.

The selection can also be set with
`?renderer=canvas2d|auto|accelerated|software`.

## Build and serve

Emscripten 4.0.1 and Ninja are required. The verified SDK location in this
repository is `/opt/emsdk`; retain the build-local cache because the shared SDK
cache may not be writable.

```sh
source /opt/emsdk/emsdk_env.sh
export EMSDK=/opt/emsdk
export EM_CACHE="$PWD/build/presets/wasm-emscripten/.emscripten-cache"
cmake --preset wasm-emscripten
cmake --build --preset wasm-emscripten --target hasciicam_wasm_sample
python3 -m http.server --directory build/presets/wasm-emscripten/examples/wasm 8000
```

Open `http://127.0.0.1:8000/` in a current Chrome/Chromium or Firefox build.
Do not open the page with `file://`: Emscripten assets and camera permissions
need a served origin. Loopback is a secure development context; use HTTPS when
hosting elsewhere. The output directory contains source-controlled `index.html`
and `main.js` plus generated `hasciicam.js` and `hasciicam.wasm`.

Select **Start camera**, grant video permission, and confirm that the 640×480
canvas shows moving ASCII. Select **Stop camera** before leaving; Stop and page
close release every camera track. The page does not send frames to a server.

## Automated Chromium smoke

The CTest browser gate is deliberately opt-in and does not use real hardware.
It starts an ephemeral localhost server, opens Chromium headlessly with fake
media and permission flags, and records browser logs under the build tree.
After acquiring the fake stream, the automated path generates deterministic
RGBA frames directly in JavaScript memory. This preserves camera lifecycle and
frame-transfer coverage without a headless video/canvas readback, which can
stall software WebGL runners. The interactive real-camera path still reads the
video through its hidden source canvas. Provide the browser executable
explicitly:

```sh
cmake --preset wasm-emscripten \\
  -DHASCIICAM_ENABLE_WASM_BROWSER_TESTS=ON \\
  -DHASCIICAM_TEST_CHROMIUM="$(command -v chromium)"
cmake --build --preset wasm-emscripten --target hasciicam_wasm_sample
ctest --output-on-failure --test-dir build/presets/wasm-emscripten -L wasm
```

The test's localhost-only `?autotest=1` mode clicks through its fake camera
path and checks runtime readiness, camera transfer, multiple rendered frames,
each explicit renderer, SDL-to-native fallback, and presentation after a
callback boundary. It also checks stop/restart/page-hide cleanup, steady-state
allocation reuse, and the permission-denied and missing-media-device error
paths. It is not real-camera coverage.

## Manual real-camera checklist

- Serve the generated directory from localhost or HTTPS, then use Chrome or
  Firefox with a real camera attached.
- Start with keyboard focus on **Start camera**; allow the permission request.
- Start with **Canvas 2D (recommended)** and verify live ASCII updates on the
  640×480 canvas.
- Stop the camera, select each other rendering system in turn, and start again.
  SDL/WebGL may be unavailable on devices that disable hardware acceleration;
  that should not prevent the native Canvas 2D selection from working.
- Select **Stop camera**, then close the page; verify the browser's camera-use
  indicator turns off.

If permission is denied, reset the site's camera permission and retry. For a
blank canvas, select **Canvas 2D (recommended)** and inspect the browser log
produced by the CTest. For cache permission errors, export the build-local
`EM_CACHE` above. A server must return `.wasm` as `application/wasm`; Python's
standard library server does so on supported Python installations.
