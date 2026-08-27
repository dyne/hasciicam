# WASM browser sample

This sample demonstrates a browser host around the shared HasciiCam core.

## Layout

- `wasm_entry.c`: C exports used by JavaScript host code.
- `index.html`: browser UI shell.
- `main.js`: acquires camera frames and submits them to core.
- `CMakeLists.txt`: Emscripten build wiring.

## Build

Use the project preset:

```sh
source /opt/emsdk/emsdk_env.sh
EMSDK=/opt/emsdk EM_CACHE="$PWD/build/presets/wasm-emscripten/.emscripten-cache" \\
cmake --preset wasm-emscripten
cmake --build --preset wasm-emscripten --target hasciicam_wasm_sample
```

The preset keeps Emscripten's SDL2 port cache under the build tree. The sample
output directory contains `hasciicam.js`, `hasciicam.wasm`, `index.html`, and
`main.js`; the HTML shell is source-controlled rather than Emscripten-generated.

## Runtime model

1. Browser `getUserMedia` captures camera frames.
2. JavaScript reads RGBA pixels from a hidden canvas.
3. JS passes RGBA bytes to C exports.
4. Core renders ASCII from the submitted frame.
5. JS prints ASCII into a `<pre>` tag.
