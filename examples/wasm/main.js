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
  let sessionFrames = 0;
  let lifecyclePhase = 0;

  const diagnostics = window.hasciicamWasmState = {
    runtimeReady: false,
    cameraReady: false,
    successfulFrames: 0,
    sdlWebglReady: false,
    canvasNonblank: false,
    canvasChanged: false,
    presentationObserved: false,
    presentationCount: 0,
    allocationCount: 0,
    freeCount: 0,
    activeRenderLoops: 0,
    maxActiveRenderLoops: 0,
    activeStreams: 0,
    maxActiveStreams: 0,
    shutdownCount: 0,
    restartCount: 0,
    errorKind: "none",
    testComplete: false,
    testPassed: false
  };
  const autotestMode = new URLSearchParams(window.location.search).get("autotest") === "1" &&
    ["127.0.0.1", "localhost", "::1"].includes(window.location.hostname);
  const autotestScenario = new URLSearchParams(window.location.search).get("scenario") || "lifecycle";

  function updateDiagnostics(changes) {
    Object.assign(diagnostics, changes);
    app.dataset.runtimeReady = String(diagnostics.runtimeReady);
    app.dataset.cameraReady = String(diagnostics.cameraReady);
    app.dataset.successfulFrames = String(diagnostics.successfulFrames);
    app.dataset.sdlWebglReady = String(diagnostics.sdlWebglReady);
    app.dataset.canvasNonblank = String(diagnostics.canvasNonblank);
    app.dataset.canvasChanged = String(diagnostics.canvasChanged);
    app.dataset.presentationObserved = String(diagnostics.presentationObserved);
    app.dataset.presentationCount = String(diagnostics.presentationCount);
    app.dataset.allocationCount = String(diagnostics.allocationCount);
    app.dataset.freeCount = String(diagnostics.freeCount);
    app.dataset.activeRenderLoops = String(diagnostics.activeRenderLoops);
    app.dataset.maxActiveRenderLoops = String(diagnostics.maxActiveRenderLoops);
    app.dataset.activeStreams = String(diagnostics.activeStreams);
    app.dataset.maxActiveStreams = String(diagnostics.maxActiveStreams);
    app.dataset.shutdownCount = String(diagnostics.shutdownCount);
    app.dataset.restartCount = String(diagnostics.restartCount);
    app.dataset.errorKind = diagnostics.errorKind;
    app.dataset.testComplete = String(diagnostics.testComplete);
    app.dataset.testPassed = String(diagnostics.testPassed);
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
    updateDiagnostics({ activeStreams: 0 });
  }

  function freeTransfer() {
    if (transferPtr) {
      window.Module._free(transferPtr);
      updateDiagnostics({ freeCount: diagnostics.freeCount + 1 });
    }
    transferPtr = 0;
    transferSize = 0;
  }

  function shutdownSession() {
    if (!sessionInitialized) return;
    sessionInitialized = false;
    api.shutdown();
    updateDiagnostics({ shutdownCount: diagnostics.shutdownCount + 1 });
  }

  function stop(message = "Camera stopped.", state = "stopped") {
    startToken += 1;
    starting = false;
    running = false;
    if (rafId) cancelAnimationFrame(rafId);
    rafId = 0;
    updateDiagnostics({ activeRenderLoops: 0 });
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

  function completeErrorAutotest(kind) {
    const clean = diagnostics.activeRenderLoops === 0 && diagnostics.activeStreams === 0 &&
      transferPtr === 0 && !sessionInitialized;
    finishAutotest({ errorKind: kind, testComplete: true, testPassed: clean });
  }

  function finishAutotest(changes) {
    updateDiagnostics(changes);
    window.setTimeout(() => window.close(), 0);
  }

  function scheduleFrame() {
    if (!running || rafId) return;
    rafId = requestAnimationFrame(frame);
    updateDiagnostics({ activeRenderLoops: 1,
      maxActiveRenderLoops: Math.max(diagnostics.maxActiveRenderLoops, 1) });
  }

  async function observePresentation() {
    if (!presentationPending || diagnostics.presentationObserved) return;
    presentationPending = false;
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
      const observed = changed && nonblank;
      updateDiagnostics({ canvasNonblank: nonblank, canvasChanged: changed,
                          presentationObserved: observed,
                          presentationCount: diagnostics.presentationCount + (observed ? 1 : 0) });
    } catch (error) {
      fatal("The browser could not verify canvas presentation.");
    }
  }

  async function frame() {
    rafId = 0;
    updateDiagnostics({ activeRenderLoops: 0 });
    if (!running) return;
    await observePresentation();
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
      updateDiagnostics({ allocationCount: diagnostics.allocationCount + 1 });
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
    sessionFrames += 1;
    if (!diagnostics.presentationObserved) presentationPending = true;
    if (autotestMode && autotestScenario === "lifecycle" && sessionFrames >= 3) {
      if (lifecyclePhase === 0) {
        lifecyclePhase = 1;
        stop("First test session stopped.", "stopped");
        stop("First test session stopped.", "stopped");
        updateDiagnostics({ restartCount: diagnostics.restartCount + 1 });
        window.setTimeout(start, 0);
        return;
      }
      if (lifecyclePhase === 1) {
        lifecyclePhase = 2;
        window.dispatchEvent(new Event("pagehide"));
        stop("Test cleanup complete.", "stopped");
        const passed = diagnostics.allocationCount === 2 && diagnostics.freeCount === 2 &&
          diagnostics.maxActiveRenderLoops === 1 && diagnostics.maxActiveStreams === 1 &&
          diagnostics.activeRenderLoops === 0 && diagnostics.activeStreams === 0 &&
          diagnostics.shutdownCount === 2 && diagnostics.restartCount === 1;
        finishAutotest({ testComplete: true, testPassed: passed });
        return;
      }
    }
    scheduleFrame();
  }

  async function start() {
    if (!diagnostics.runtimeReady || running || starting) return;
    if (!window.isSecureContext) {
      fatal("Camera access requires HTTPS or localhost.");
      return;
    }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia ||
        (autotestMode && autotestScenario === "missing-media")) {
      fatal("No camera is available in this browser.");
      if (autotestMode && autotestScenario === "missing-media") completeErrorAutotest("missing-media");
      return;
    }
    const token = ++startToken;
    starting = true;
    setControls();
    setStatus("Requesting camera permission…", "loading");
    try {
      if (autotestMode && autotestScenario === "denied") {
        const denied = new Error("Camera permission denied for deterministic test");
        denied.name = "NotAllowedError";
        throw denied;
      }
      const acquired = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
      if (token !== startToken) {
        acquired.getTracks().forEach((track) => track.stop());
        return;
      }
      stream = acquired;
      updateDiagnostics({ activeStreams: 1,
        maxActiveStreams: Math.max(diagnostics.maxActiveStreams, 1) });
      video.srcObject = stream;
      await new Promise((resolve) => video.addEventListener("loadedmetadata", resolve, { once: true }));
      if (token !== startToken) return;
      await video.play();
      if (token !== startToken) return;
      sessionFrames = 0;
      updateDiagnostics({ cameraReady: true, canvasNonblank: false,
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
      scheduleFrame();
    } catch (error) {
      if (token !== startToken) return;
      starting = false;
      const denied = error && (error.name === "NotAllowedError" || error.name === "SecurityError");
      fatal(denied ? "Camera permission was denied. Allow access and try again." : "No camera could be started. Check that one is connected and not in use.");
      if (autotestMode && autotestScenario === "denied") completeErrorAutotest("denied");
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
