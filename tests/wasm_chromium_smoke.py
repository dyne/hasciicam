#!/usr/bin/env python3
"""CTest wrapper for the opt-in Emscripten SDL canvas smoke test."""

import argparse
import http.server
import os
from pathlib import Path
import re
import socketserver
import subprocess
import sys
import threading


class QuietStaticServer(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format, *_args):
        pass


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--chromium", required=True)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    return parser.parse_args()


def main():
    args = parse_args()
    chromium = Path(args.chromium)
    if not chromium.is_file() or not os.access(chromium, os.X_OK):
        raise RuntimeError(f"Chromium executable is unavailable: {chromium}")
    if not (args.root / "index.html").is_file():
        raise RuntimeError(f"WASM sample assets are unavailable: {args.root}")

    args.artifacts.mkdir(parents=True, exist_ok=True)
    log_path = args.artifacts / "chromium.log"
    screenshot_path = args.artifacts / "canvas.png"

    handler = lambda *handler_args, **handler_kwargs: QuietStaticServer(
        *handler_args, directory=str(args.root), **handler_kwargs)
    with socketserver.TCPServer(("127.0.0.1", 0), handler) as server:
        server.allow_reuse_address = True
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = f"http://127.0.0.1:{server.server_address[1]}/index.html?autotest=1"
        command = [
            str(chromium), "--headless=new", "--no-sandbox", "--disable-dev-shm-usage",
            "--use-fake-device-for-media-stream", "--use-fake-ui-for-media-stream",
            "--use-gl=angle", "--use-angle=swiftshader", "--enable-webgl",
            "--ignore-gpu-blocklist", "--run-all-compositor-stages-before-draw",
            "--virtual-time-budget=15000", "--window-size=1280,800",
            f"--screenshot={screenshot_path}", "--dump-dom", url,
        ]
        try:
            completed = subprocess.run(
                command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, timeout=45, check=False)
        except subprocess.TimeoutExpired as error:
            log_path.write_text(error.stdout or "Chromium timed out.\n", encoding="utf-8")
            raise RuntimeError("Chromium timed out after 45 seconds") from error
        finally:
            server.shutdown()
            thread.join(timeout=5)

    log_path.write_text(completed.stdout, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"Chromium exited with status {completed.returncode}; see {log_path}")
    if "Uncaught" in completed.stdout or "ERROR:CONSOLE" in completed.stdout:
        raise RuntimeError(f"Chromium reported a page exception; see {log_path}")
    if not screenshot_path.is_file() or screenshot_path.stat().st_size == 0:
        raise RuntimeError(f"Chromium did not produce a canvas screenshot: {screenshot_path}")

    states = dict(re.findall(r'data-([a-z-]+)="([^"]*)"', completed.stdout))
    required = {
        "runtime-ready": "true", "camera-ready": "true", "sdl-webgl-ready": "true",
        "presentation-observed": "true",
    }
    for name, expected in required.items():
        if states.get(name) != expected:
            raise RuntimeError(f"browser state {name}={states.get(name)!r}, expected {expected!r}; see {log_path}")
    try:
        frames = int(states.get("successful-frames", "0"))
    except ValueError as error:
        raise RuntimeError(f"invalid successful-frame count; see {log_path}") from error
    if frames < 2:
        raise RuntimeError(f"browser rendered only {frames} fake-camera frames; see {log_path}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"wasm Chromium smoke failed: {error}", file=sys.stderr)
        sys.exit(1)
