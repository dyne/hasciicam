#!/usr/bin/env python3
"""CTest wrapper for the opt-in Emscripten browser renderer smoke test."""

import argparse
import http.server
import json
import os
from pathlib import Path
import socketserver
import subprocess
import sys
import threading
from urllib.parse import urlparse


LIFECYCLE_TIMEOUT_SECONDS = 90


class SmokeServer(socketserver.TCPServer):
    allow_reuse_address = True

    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.completion = None
        self.completion_event = threading.Event()


class QuietStaticServer(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format, *_args):
        pass

    def do_POST(self):
        if urlparse(self.path).path != "/_hasciicam_test_complete":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            self.server.completion = json.loads(self.rfile.read(length))
        except (ValueError, json.JSONDecodeError):
            self.send_error(400)
            return
        self.server.completion_event.set()
        self.send_response(204)
        self.end_headers()


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--chromium", required=True)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    return parser.parse_args()


def run_scenario(chromium, root, artifacts, scenario, renderer="auto"):
    case_name = scenario if renderer == "auto" else f"{scenario}-{renderer}"
    renderer_request = "auto" if renderer == "fallback" else renderer
    log_path = artifacts / f"chromium-{case_name}.log"
    profile_dir = artifacts / f"chromium-profile-{case_name}"
    profile_dir.mkdir(parents=True, exist_ok=True)
    handler = lambda *handler_args, **handler_kwargs: QuietStaticServer(
        *handler_args, directory=str(root), **handler_kwargs)
    with SmokeServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        renderer_query = "" if renderer == "auto" else f"&renderer={renderer_request}"
        url = (f"http://127.0.0.1:{server.server_address[1]}/index.html"
               f"?autotest=1&scenario={scenario}{renderer_query}")
        command = [
            str(chromium), "--headless=new", "--no-sandbox", "--disable-dev-shm-usage",
            "--no-first-run", "--no-default-browser-check",
            f"--user-data-dir={profile_dir}",
            "--use-fake-device-for-media-stream", "--use-fake-ui-for-media-stream",
        ]
        if renderer == "fallback":
            command.append("--disable-webgl")
        else:
            command.extend([
                "--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                "--enable-webgl", "--ignore-gpu-blocklist",
            ])
        command.extend(["--run-all-compositor-stages-before-draw", "--window-size=1280,800", url])
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True)
        try:
            if not server.completion_event.wait(LIFECYCLE_TIMEOUT_SECONDS):
                process.terminate()
                output, _ = process.communicate(timeout=5)
                log_path.write_text(output or "Chromium timed out.\n", encoding="utf-8")
                raise RuntimeError(
                    f"Chromium {scenario} timed out after {LIFECYCLE_TIMEOUT_SECONDS} seconds")
            process.terminate()
            output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired as error:
            process.kill()
            output, _ = process.communicate()
            log_path.write_text(output or "Chromium did not terminate.\n", encoding="utf-8")
            raise RuntimeError(
                f"Chromium {scenario} did not terminate after completion") from error
        finally:
            server.shutdown()
            thread.join(timeout=5)

    log_path.write_text(output, encoding="utf-8")
    completion = server.completion
    completion_path = artifacts / f"completion-{case_name}.json"
    completion_path.write_text(
        json.dumps(completion, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    if "Uncaught" in output or "ERROR:CONSOLE" in output:
        raise RuntimeError(f"Chromium {scenario} reported a page exception; see {log_path}")
    if not isinstance(completion, dict) or completion.get("scenario") != scenario:
        raise RuntimeError(
            f"browser scenario {scenario} sent an invalid completion signal: "
            f"{completion!r}; see {completion_path}")
    states = completion.get("states", {})
    if states.get("testComplete") != "true" or states.get("testPassed") != "true":
        raise RuntimeError(
            f"browser scenario {scenario} did not complete successfully "
            f"(error-kind={states.get('errorKind')!r}, "
            f"successful-frames={states.get('successfulFrames')!r}, "
            f"renderer-message={states.get('rendererMessage')!r}); "
            f"see {completion_path}")
    if scenario == "lifecycle":
        expected = {
            "allocationCount": "2", "freeCount": "2", "activeRenderLoops": "0",
            "maxActiveRenderLoops": "1", "activeStreams": "0", "maxActiveStreams": "1",
            "shutdownCount": "2", "restartCount": "1", "errorKind": "none",
        }
        for name, value in expected.items():
            if states.get(name) != value:
                raise RuntimeError(f"browser state {name}={states.get(name)!r}, expected {value!r}; see {log_path}")
        if int(states.get("successfulFrames", "0")) < 6:
            raise RuntimeError(f"browser did not render both lifecycle sessions; see {log_path}")
        if int(states.get("presentationCount", "0")) < 1:
            raise RuntimeError(f"browser did not observe canvas presentation; see {log_path}")
        if renderer in ("canvas2d", "fallback-native"):
            expected_renderer = "canvas2d"
            if int(states.get("canvasDrawCount", "0")) < 6:
                raise RuntimeError(f"browser did not draw Canvas 2D lifecycle frames; see {log_path}")
        elif renderer in ("software", "fallback", "fallback-lost"):
            expected_renderer = "software"
        else:
            expected_renderer = "accelerated"
        if states.get("rendererBackend") != expected_renderer:
            raise RuntimeError(
                f"browser renderer={states.get('rendererBackend')!r}, "
                f"expected {expected_renderer!r}; see {log_path}")
    elif states.get("errorKind") != scenario:
        raise RuntimeError(f"browser error-kind={states.get('errorKind')!r}, expected {scenario!r}; see {log_path}")


def main():
    args = parse_args()
    chromium = Path(args.chromium)
    if not chromium.is_file() or not os.access(chromium, os.X_OK):
        raise RuntimeError(f"Chromium executable is unavailable: {chromium}")
    if not (args.root / "index.html").is_file():
        raise RuntimeError(f"WASM sample assets are unavailable: {args.root}")
    if 'id="renderer"' in (args.root / "index.html").read_text(encoding="utf-8"):
        raise RuntimeError("WASM sample still exposes a renderer selector")

    args.artifacts.mkdir(parents=True, exist_ok=True)
    run_scenario(chromium, args.root, args.artifacts, "lifecycle")
    run_scenario(chromium, args.root, args.artifacts, "lifecycle", "canvas2d")
    run_scenario(chromium, args.root, args.artifacts, "lifecycle", "software")
    run_scenario(chromium, args.root, args.artifacts, "lifecycle", "fallback")
    run_scenario(chromium, args.root, args.artifacts, "lifecycle", "fallback-lost")
    run_scenario(chromium, args.root, args.artifacts, "lifecycle", "fallback-native")
    for scenario in ("denied", "missing-media"):
        run_scenario(chromium, args.root, args.artifacts, scenario)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"wasm Chromium smoke failed: {error}", file=sys.stderr)
        sys.exit(1)
