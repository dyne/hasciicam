(() => {
  "use strict";

  const SOURCE_WIDTH = 320;
  const SOURCE_HEIGHT = 240;
  const CANVAS_WIDTH = 640;
  const CANVAS_HEIGHT = 477;
  const ASCII_WIDTH = 80;
  const ASCII_HEIGHT = 53;
  const RENDERER_MODES = { accelerated: 0, software: 1, canvas2d: 2 };
  const app = document.getElementById("hasciicam-app");
  const video = document.getElementById("camera-video");
  const sourceCanvas = document.getElementById("source-canvas");
  let visibleCanvas = document.getElementById("canvas");
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
  let sessionFrames = 0;
  let lifecyclePhase = 0;
  let autotestPixels = null;
  let rendererBackend = "none";

  const diagnostics = window.hasciicamWasmState = {
    runtimeReady: false,
    cameraReady: false,
    successfulFrames: 0,
    sdlWebglReady: false,
    canvasNonblank: false,
    canvasChanged: false,
    presentationObserved: false,
    presentationCount: 0,
    canvasDrawCount: 0,
    allocationCount: 0,
    freeCount: 0,
    activeRenderLoops: 0,
    maxActiveRenderLoops: 0,
    activeStreams: 0,
    maxActiveStreams: 0,
    shutdownCount: 0,
    restartCount: 0,
    rendererMessage: "",
    rendererBackend: "none",
    errorKind: "none",
    testComplete: false,
    testPassed: false
  };
  const urlParams = new URLSearchParams(window.location.search);
  const autotestMode = urlParams.get("autotest") === "1" &&
    ["127.0.0.1", "localhost", "::1"].includes(window.location.hostname);
  const autotestScenario = urlParams.get("scenario") || "lifecycle";
  const rendererOption = autotestMode ? urlParams.get("renderer") : null;

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
    app.dataset.canvasDrawCount = String(diagnostics.canvasDrawCount);
    app.dataset.allocationCount = String(diagnostics.allocationCount);
    app.dataset.freeCount = String(diagnostics.freeCount);
    app.dataset.activeRenderLoops = String(diagnostics.activeRenderLoops);
    app.dataset.maxActiveRenderLoops = String(diagnostics.maxActiveRenderLoops);
    app.dataset.activeStreams = String(diagnostics.activeStreams);
    app.dataset.maxActiveStreams = String(diagnostics.maxActiveStreams);
    app.dataset.shutdownCount = String(diagnostics.shutdownCount);
    app.dataset.restartCount = String(diagnostics.restartCount);
    app.dataset.rendererMessage = diagnostics.rendererMessage;
    app.dataset.rendererBackend = diagnostics.rendererBackend;
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
    updateDiagnostics({ cameraReady: false, sdlWebglReady: false, canvasNonblank: false,
                        canvasChanged: false, presentationObserved: false });
    setControls();
    setStatus(message, state);
  }

  function fatal(message, kind = "runtime") {
    stop(message, "error");
    if (autotestMode && autotestScenario === "lifecycle") {
      finishAutotest({ errorKind: kind, testComplete: true, testPassed: false });
    }
  }

  function completeErrorAutotest(kind) {
    const clean = diagnostics.activeRenderLoops === 0 && diagnostics.activeStreams === 0 &&
      transferPtr === 0 && !sessionInitialized;
    finishAutotest({ errorKind: kind, testComplete: true, testPassed: clean });
  }

  function finishAutotest(changes) {
    updateDiagnostics(changes);
    const completion = {
      scenario: autotestScenario,
      states: Object.fromEntries(Object.entries(app.dataset))
    };
    window.fetch("/_hasciicam_test_complete", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(completion)
    }).finally(() => window.close());
  }

  function scheduleFrame() {
    if (!running || rafId) return;
    rafId = requestAnimationFrame(frame);
    updateDiagnostics({ activeRenderLoops: 1,
      maxActiveRenderLoops: Math.max(diagnostics.maxActiveRenderLoops, 1) });
  }

  function replaceVisibleCanvas() {
    const replacement = visibleCanvas.cloneNode(false);
    replacement.width = CANVAS_WIDTH;
    replacement.height = CANVAS_HEIGHT;
    visibleCanvas.replaceWith(replacement);
    visibleCanvas = replacement;
    window.Module.canvas = visibleCanvas;
  }

  function getRendererContext(backend) {
    return backend === "accelerated" ?
      visibleCanvas.getContext("webgl") : visibleCanvas.getContext("2d");
  }

  function rendererContextIsUsable(backend, context) {
    return Boolean(context && (backend !== "accelerated" || !context.isContextLost()));
  }

  function drawCanvas2dFrame() {
    const textPtr = api.asciiText();
    const width = api.asciiWidth();
    const height = api.asciiHeight();
    const context = visibleCanvas.getContext("2d");
    if (!textPtr || !context || width <= 0 || height <= 0) return false;

    const cellWidth = visibleCanvas.width / width;
    const cellHeight = visibleCanvas.height / height;
    context.fillStyle = "#000";
    context.fillRect(0, 0, visibleCanvas.width, visibleCanvas.height);
    context.fillStyle = "#f4f7fa";
    context.font = `${cellHeight}px "Courier New", ui-monospace, monospace`;
    context.textBaseline = "top";
    const glyphWidth = context.measureText("M").width || cellWidth;
    context.save();
    context.scale(cellWidth / glyphWidth, 1);
    for (let row = 0; row < height; row += 1) {
      const start = textPtr + row * width;
      const line = String.fromCharCode(...window.Module.HEAPU8.subarray(start, start + width));
      context.fillText(line, 0, row * cellHeight);
    }
    context.restore();
    updateDiagnostics({ canvasDrawCount: diagnostics.canvasDrawCount + 1 });
    return true;
  }

  function observePresentation() {
    if (!presentationPending || diagnostics.presentationObserved) return;
    presentationPending = false;
    const context = getRendererContext(rendererBackend);
    const observed = rendererContextIsUsable(rendererBackend, context) &&
      (rendererBackend !== "canvas2d" || diagnostics.canvasDrawCount > 0);
    updateDiagnostics({ canvasNonblank: observed, canvasChanged: observed,
                        presentationObserved: observed,
                        presentationCount: diagnostics.presentationCount + (observed ? 1 : 0) });
  }

  function getAutotestPixels() {
    if (!autotestPixels) {
      autotestPixels = new Uint8Array(SOURCE_WIDTH * SOURCE_HEIGHT * 4);
    }
    const phase = diagnostics.successfulFrames * 17;
    for (let y = 0; y < SOURCE_HEIGHT; y += 1) {
      for (let x = 0; x < SOURCE_WIDTH; x += 1) {
        const offset = (y * SOURCE_WIDTH + x) * 4;
        autotestPixels[offset] = (x + phase) & 0xff;
        autotestPixels[offset + 1] = (y * 2 + phase) & 0xff;
        autotestPixels[offset + 2] = (x + y + phase * 2) & 0xff;
        autotestPixels[offset + 3] = 0xff;
      }
    }
    return autotestPixels;
  }

  async function frame() {
    rafId = 0;
    updateDiagnostics({ activeRenderLoops: 0 });
    if (!running) return;
    observePresentation();
    if (!running) return;
    let pixels;
    if (autotestMode) {
      pixels = getAutotestPixels();
    } else {
      sourceContext.drawImage(video, 0, 0, SOURCE_WIDTH, SOURCE_HEIGHT);
      pixels = sourceContext.getImageData(0, 0, SOURCE_WIDTH, SOURCE_HEIGHT).data;
    }
    if (pixels.byteLength > transferSize) {
      freeTransfer();
      transferPtr = window.Module._malloc(pixels.byteLength);
      transferSize = pixels.byteLength;
      if (!transferPtr) {
        fatal("The renderer could not reserve frame memory.", "allocation");
        return;
      }
      updateDiagnostics({ allocationCount: diagnostics.allocationCount + 1 });
    }
    window.Module.HEAPU8.set(pixels, transferPtr);
    if (!api.submit(transferPtr, pixels.byteLength, SOURCE_WIDTH, SOURCE_HEIGHT, SOURCE_WIDTH * 4)) {
      fatal("The renderer rejected a camera frame.", "frame-submit");
      return;
    }
    if (!api.render()) {
      fatal("The renderer stopped while drawing a frame.", "frame-render");
      return;
    }
    if (rendererBackend === "canvas2d" && !drawCanvas2dFrame()) {
      fatal("Canvas 2D could not draw the ASCII frame.", "canvas-draw");
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
      fatal("Camera access requires HTTPS or localhost.", "insecure-context");
      return;
    }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia ||
        (autotestMode && autotestScenario === "missing-media")) {
      fatal("No camera is available in this browser.", "missing-media");
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
      const simulateLostContext = autotestMode && rendererOption === "fallback-lost";
      const simulateNativeFallback = autotestMode && rendererOption === "fallback-native";
      const simulateFallback = simulateLostContext || simulateNativeFallback;
      const rendererRequest = !simulateFallback && autotestMode &&
        ["canvas2d", "accelerated", "software"].includes(rendererOption) ?
        rendererOption : "auto";
      const attempts = rendererRequest === "auto" ?
        ["accelerated", "software", "canvas2d"] : [rendererRequest];
      let initialized = false;
      let rendererUsable = false;
      replaceVisibleCanvas();
      for (let index = 0; index < attempts.length; index += 1) {
        if (index > 0) {
          api.shutdown();
          replaceVisibleCanvas();
        }
        rendererBackend = attempts[index];
        initialized = api.init(SOURCE_WIDTH, SOURCE_HEIGHT, ASCII_WIDTH, ASCII_HEIGHT,
                               RENDERER_MODES[rendererBackend]);
        const rendererContext = initialized ? getRendererContext(rendererBackend) : null;
        rendererUsable = rendererContextIsUsable(rendererBackend, rendererContext);
        if (simulateLostContext && rendererBackend === "accelerated" && rendererUsable) {
          const loseContext = rendererContext.getExtension("WEBGL_lose_context");
          if (loseContext) loseContext.loseContext();
          rendererUsable = false;
        }
        if (simulateNativeFallback && rendererBackend !== "canvas2d") {
          rendererUsable = false;
        }
        if (initialized && rendererUsable) break;
      }
      if (!initialized || !rendererUsable) {
        const detail = diagnostics.rendererMessage ? ` ${diagnostics.rendererMessage}` : "";
        fatal(`Could not initialize the selected rendering system.${detail}`, "renderer-init");
        return;
      }
      running = true;
      starting = false;
      updateDiagnostics({ sdlWebglReady: rendererBackend === "accelerated",
                          rendererBackend, rendererMessage: "" });
      setControls();
      const rendererLabels = {
        accelerated: "SDL / WebGL",
        software: "SDL software",
        canvas2d: "native Canvas 2D"
      };
      setStatus(`Camera is running with ${rendererLabels[rendererBackend]}.`);
      scheduleFrame();
    } catch (error) {
      if (token !== startToken) return;
      starting = false;
      const denied = error && (error.name === "NotAllowedError" || error.name === "SecurityError");
      fatal(denied ? "Camera permission was denied. Allow access and try again." : "No camera could be started. Check that one is connected and not in use.",
            denied ? "camera-denied" : "camera-start");
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
    printErr(message) {
      const rendererMessage = String(message);
      updateDiagnostics({ rendererMessage });
      setStatus(`Renderer message: ${rendererMessage}`, "error");
    },
    onAbort: (message) => runtimeFailure(`Renderer failed to load: ${message}`),
    onRuntimeInitialized() {
      api = {
        init: window.Module.cwrap("hasciicam_wasm_init", "number", ["number", "number", "number", "number", "number"]),
        submit: window.Module.cwrap("hasciicam_wasm_submit_rgba", "number", ["number", "number", "number", "number", "number"]),
        render: window.Module.cwrap("hasciicam_wasm_render", "number", []),
        asciiText: window.Module.cwrap("hasciicam_wasm_ascii_text", "number", []),
        asciiWidth: window.Module.cwrap("hasciicam_wasm_ascii_width", "number", []),
        asciiHeight: window.Module.cwrap("hasciicam_wasm_ascii_height", "number", []),
        shutdown: window.Module.cwrap("hasciicam_wasm_shutdown", null, [])
      };
      updateDiagnostics({ runtimeReady: true });
      setControls();
      setStatus("Renderer ready. Start the camera when you are ready.");
      if (autotestMode) window.setTimeout(start, 0);
    }
  };
})();
