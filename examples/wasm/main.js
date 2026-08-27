(() => {
  "use strict";

  const SOURCE_WIDTH = 320;
  const SOURCE_HEIGHT = 240;
  const app = document.getElementById("hasciicam-app");
  const video = document.getElementById("camera-video");
  const sourceCanvas = document.getElementById("source-canvas");
  const visibleCanvas = document.getElementById("canvas");
  const startButton = document.getElementById("start");
  const stopButton = document.getElementById("stop");
  const status = document.getElementById("status");
  const sourceContext = sourceCanvas.getContext("2d", { willReadFrequently: true });
  let api = null;
  let stream = null;
  let transferPtr = 0;
  let transferSize = 0;
  let rafId = 0;
  let running = false;
  let starting = false;
  let startToken = 0;
  let sessionInitialized = false;
  let presentationPending = false;
  let initialCanvasSnapshot = "";

  const diagnostics = window.hasciicamWasmState = {
    runtimeReady: false,
    cameraReady: false,
    successfulFrames: 0,
    sdlWebglReady: false,
    canvasNonblank: false,
    canvasChanged: false,
    presentationObserved: false
  };
  const autotestMode = new URLSearchParams(window.location.search).get("autotest") === "1" &&
    ["127.0.0.1", "localhost", "::1"].includes(window.location.hostname);

  function updateDiagnostics(changes) {
    Object.assign(diagnostics, changes);
    app.dataset.runtimeReady = String(diagnostics.runtimeReady);
    app.dataset.cameraReady = String(diagnostics.cameraReady);
    app.dataset.successfulFrames = String(diagnostics.successfulFrames);
    app.dataset.sdlWebglReady = String(diagnostics.sdlWebglReady);
    app.dataset.canvasNonblank = String(diagnostics.canvasNonblank);
    app.dataset.canvasChanged = String(diagnostics.canvasChanged);
    app.dataset.presentationObserved = String(diagnostics.presentationObserved);
  }

  function setStatus(message, state = "ready") {
    status.textContent = message;
    status.dataset.state = state;
  }

  function setControls() {
    startButton.disabled = !diagnostics.runtimeReady || running || starting;
    stopButton.disabled = !running && !starting;
  }

  function stopTracks() {
    if (stream) stream.getTracks().forEach((track) => track.stop());
    stream = null;
    video.srcObject = null;
  }

  function freeTransfer() {
    if (transferPtr) window.Module._free(transferPtr);
    transferPtr = 0;
    transferSize = 0;
  }

  function shutdownSession() {
    if (!sessionInitialized) return;
    sessionInitialized = false;
    api.shutdown();
  }

  function stop(message = "Camera stopped.", state = "stopped") {
    startToken += 1;
    starting = false;
    running = false;
    if (rafId) cancelAnimationFrame(rafId);
    rafId = 0;
    stopTracks();
    shutdownSession();
    freeTransfer();
    presentationPending = false;
    initialCanvasSnapshot = "";
    updateDiagnostics({ cameraReady: false, sdlWebglReady: false, canvasNonblank: false, canvasChanged: false, presentationObserved: false });
    setControls();
    setStatus(message, state);
  }

  function fatal(message) {
    stop(message, "error");
  }

  function schedulePresentationObservation() {
    if (presentationPending || diagnostics.presentationObserved) return;
    presentationPending = true;
    requestAnimationFrame(async () => {
      presentationPending = false;
      if (!running) return;
      try {
        const snapshot = visibleCanvas.toDataURL("image/png");
        const changed = snapshot !== initialCanvasSnapshot;
        const bitmap = await createImageBitmap(visibleCanvas);
        const probe = document.createElement("canvas");
        probe.width = bitmap.width;
        probe.height = bitmap.height;
        const probeContext = probe.getContext("2d");
        probeContext.drawImage(bitmap, 0, 0);
        bitmap.close();
        const pixels = probeContext.getImageData(0, 0, probe.width, probe.height).data;
        let nonblank = false;
        for (let offset = 4; offset < pixels.length; offset += 4) {
          if (pixels[offset] !== pixels[0] || pixels[offset + 1] !== pixels[1] ||
              pixels[offset + 2] !== pixels[2] || pixels[offset + 3] !== pixels[3]) {
            nonblank = true;
            break;
          }
        }
        updateDiagnostics({ canvasNonblank: nonblank, canvasChanged: changed,
                            presentationObserved: changed && nonblank });
      } catch (error) {
        fatal("The browser could not verify canvas presentation.");
      }
    });
  }

  function frame() {
    if (!running) return;
    sourceContext.drawImage(video, 0, 0, SOURCE_WIDTH, SOURCE_HEIGHT);
    const pixels = sourceContext.getImageData(0, 0, SOURCE_WIDTH, SOURCE_HEIGHT).data;
    if (pixels.byteLength > transferSize) {
      freeTransfer();
      transferPtr = window.Module._malloc(pixels.byteLength);
      transferSize = pixels.byteLength;
      if (!transferPtr) {
        fatal("The renderer could not reserve frame memory.");
        return;
      }
    }
    window.Module.HEAPU8.set(pixels, transferPtr);
    if (!api.submit(transferPtr, pixels.byteLength, SOURCE_WIDTH, SOURCE_HEIGHT, SOURCE_WIDTH * 4)) {
      fatal("The renderer rejected a camera frame.");
      return;
    }
    if (!api.render()) {
      fatal("The SDL renderer stopped while drawing a frame.");
      return;
    }
    updateDiagnostics({ successfulFrames: diagnostics.successfulFrames + 1 });
    schedulePresentationObservation();
    rafId = requestAnimationFrame(frame);
  }

  async function start() {
    if (!diagnostics.runtimeReady || running || starting) return;
    if (!window.isSecureContext) {
      fatal("Camera access requires HTTPS or localhost.");
      return;
    }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      fatal("No camera is available in this browser.");
      return;
    }
    const token = ++startToken;
    starting = true;
    setControls();
    setStatus("Requesting camera permission…", "loading");
    try {
      const acquired = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
      if (token !== startToken) {
        acquired.getTracks().forEach((track) => track.stop());
        return;
      }
      stream = acquired;
      video.srcObject = stream;
      await new Promise((resolve) => video.addEventListener("loadedmetadata", resolve, { once: true }));
      if (token !== startToken) return;
      await video.play();
      if (token !== startToken) return;
      updateDiagnostics({ cameraReady: true, successfulFrames: 0, canvasNonblank: false,
                          canvasChanged: false, presentationObserved: false });
      sessionInitialized = true;
      if (!api.init(SOURCE_WIDTH, SOURCE_HEIGHT, 80, 30)) {
        fatal("SDL could not initialize the visible canvas.");
        return;
      }
      const webgl = visibleCanvas.getContext("webgl");
      if (!webgl || !(webgl instanceof WebGLRenderingContext)) {
        fatal("SDL did not create the required WebGL canvas renderer.");
        return;
      }
      initialCanvasSnapshot = visibleCanvas.toDataURL("image/png");
      running = true;
      starting = false;
      updateDiagnostics({ sdlWebglReady: true });
      setControls();
      setStatus("Camera is running. Rendering locally.");
      rafId = requestAnimationFrame(frame);
    } catch (error) {
      if (token !== startToken) return;
      starting = false;
      const denied = error && (error.name === "NotAllowedError" || error.name === "SecurityError");
      fatal(denied ? "Camera permission was denied. Allow access and try again." : "No camera could be started. Check that one is connected and not in use.");
    }
  }

  function runtimeFailure(message) {
    updateDiagnostics({ runtimeReady: false });
    fatal(message);
  }

  startButton.addEventListener("click", start);
  stopButton.addEventListener("click", () => stop());
  window.addEventListener("pagehide", () => stop("Camera stopped because this page is closing.", "stopped"));

  window.Module = {
    canvas: visibleCanvas,
    printErr: (message) => setStatus(`Renderer message: ${message}`, "error"),
    onAbort: (message) => runtimeFailure(`Renderer failed to load: ${message}`),
    onRuntimeInitialized() {
      api = {
        init: window.Module.cwrap("hasciicam_wasm_init", "number", ["number", "number", "number", "number"]),
        submit: window.Module.cwrap("hasciicam_wasm_submit_rgba", "number", ["number", "number", "number", "number", "number"]),
        render: window.Module.cwrap("hasciicam_wasm_render", "number", []),
        shutdown: window.Module.cwrap("hasciicam_wasm_shutdown", null, [])
      };
      updateDiagnostics({ runtimeReady: true });
      setControls();
      setStatus("Renderer ready. Start the camera when you are ready.");
      if (autotestMode) window.setTimeout(start, 0);
    }
  };
})();
