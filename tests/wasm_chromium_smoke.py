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


LIFECYCLE_TIMEOUT_SECONDS = 90


class QuietStaticServer(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format, *_args):
        pass


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--chromium", required=True)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    return parser.parse_args()


def run_scenario(chromium, root, artifacts, scenario):
    log_path = artifacts / f"chromium-{scenario}.log"
    screenshot_path = artifacts / f"canvas-{scenario}.png"
    profile_dir = artifacts / f"chromium-profile-{scenario}"
    profile_dir.mkdir(parents=True, exist_ok=True)
    handler = lambda *handler_args, **handler_kwargs: QuietStaticServer(
        *handler_args, directory=str(root), **handler_kwargs)
    with socketserver.TCPServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = f"http://127.0.0.1:{server.server_address[1]}/index.html?autotest=1&scenario={scenario}"
        command = [
            str(chromium), "--headless=new", "--no-sandbox", "--disable-dev-shm-usage",
            "--disable-gpu", "--no-first-run", "--no-default-browser-check",
            f"--user-data-dir={profile_dir}",
            "--use-fake-device-for-media-stream", "--use-fake-ui-for-media-stream",
            "--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
            "--enable-webgl",
            "--ignore-gpu-blocklist", "--run-all-compositor-stages-before-draw",
            "--virtual-time-budget=15000", "--window-size=1280,800",
            f"--screenshot={screenshot_path}", "--dump-dom", url,
        ]
        try:
            completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, timeout=LIFECYCLE_TIMEOUT_SECONDS, check=False)
        except subprocess.TimeoutExpired as error:
            output = error.stdout or "Chromium timed out.\n"
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            log_path.write_text(output, encoding="utf-8")
            raise RuntimeError(
                f"Chromium {scenario} timed out after {LIFECYCLE_TIMEOUT_SECONDS} seconds") from error
        finally:
            server.shutdown()
            thread.join(timeout=5)

    log_path.write_text(completed.stdout, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"Chromium {scenario} exited with status {completed.returncode}; see {log_path}")
    if "Uncaught" in completed.stdout or "ERROR:CONSOLE" in completed.stdout:
        raise RuntimeError(f"Chromium {scenario} reported a page exception; see {log_path}")
    if not screenshot_path.is_file() or screenshot_path.stat().st_size == 0:
        raise RuntimeError(f"Chromium did not produce a {scenario} screenshot: {screenshot_path}")

    states = dict(re.findall(r'data-([a-z-]+)="([^"]*)"', completed.stdout))
    if states.get("test-complete") != "true" or states.get("test-passed") != "true":
        raise RuntimeError(f"browser scenario {scenario} did not complete successfully; see {log_path}")
    if scenario == "lifecycle":
        expected = {
            "allocation-count": "2", "free-count": "2", "active-render-loops": "0",
            "max-active-render-loops": "1", "active-streams": "0", "max-active-streams": "1",
            "shutdown-count": "2", "restart-count": "1", "error-kind": "none",
        }
        for name, value in expected.items():
            if states.get(name) != value:
                raise RuntimeError(f"browser state {name}={states.get(name)!r}, expected {value!r}; see {log_path}")
        if int(states.get("successful-frames", "0")) < 6:
            raise RuntimeError(f"browser did not render both lifecycle sessions; see {log_path}")
        if int(states.get("presentation-count", "0")) < 1:
            raise RuntimeError(f"browser did not observe SDL canvas presentation; see {log_path}")
    elif states.get("error-kind") != scenario:
        raise RuntimeError(f"browser error-kind={states.get('error-kind')!r}, expected {scenario!r}; see {log_path}")


def main():
    args = parse_args()
    chromium = Path(args.chromium)
    if not chromium.is_file() or not os.access(chromium, os.X_OK):
        raise RuntimeError(f"Chromium executable is unavailable: {chromium}")
    if not (args.root / "index.html").is_file():
        raise RuntimeError(f"WASM sample assets are unavailable: {args.root}")

    args.artifacts.mkdir(parents=True, exist_ok=True)
    for scenario in ("lifecycle", "denied", "missing-media"):
        run_scenario(chromium, args.root, args.artifacts, scenario)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"wasm Chromium smoke failed: {error}", file=sys.stderr)
        sys.exit(1)
