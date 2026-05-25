const video = document.getElementById("cam");
const canvas = document.getElementById("src");
const ascii = document.getElementById("ascii");
const ctx = canvas.getContext("2d");

function bootstrap(Module) {
  const init = Module.cwrap("hasciicam_wasm_init", "number", ["number", "number", "number", "number"]);
  const submit = Module.cwrap("hasciicam_wasm_submit_rgba", "number", ["number", "number", "number", "number", "number"]);
  const render = Module.cwrap("hasciicam_wasm_render", "number", []);
  const textPtr = Module.cwrap("hasciicam_wasm_ascii_text", "number", []);
  const textW = Module.cwrap("hasciicam_wasm_ascii_width", "number", []);
  const textH = Module.cwrap("hasciicam_wasm_ascii_height", "number", []);

  async function start() {
    const stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
    video.srcObject = stream;
    await video.play();
    canvas.width = 320;
    canvas.height = 240;
    if (!init(canvas.width, canvas.height, 80, 40)) {
      return;
    }
    tick();
  }

  function tick() {
    ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
    const image = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const ptr = Module._malloc(image.data.length);
    Module.HEAPU8.set(image.data, ptr);
    submit(ptr, image.data.length, canvas.width, canvas.height, canvas.width * 4);
    Module._free(ptr);
    if (render()) {
      const p = textPtr();
      const w = textW();
      const h = textH();
      const raw = Module.UTF8ToString(p);
      ascii.textContent = raw.slice(0, w * h);
    }
    requestAnimationFrame(tick);
  }

  start().catch((err) => {
    ascii.textContent = String(err);
  });
}

if (typeof Module === "undefined") {
  window.Module = {};
}
window.Module.onRuntimeInitialized = function () {
  bootstrap(window.Module);
};
