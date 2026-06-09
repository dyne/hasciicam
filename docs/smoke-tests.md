# Smoke Tests

This file collects short manual checks for supported host paths.

## Deterministic CLI (no camera required)

Build and run full deterministic CLI suite:

```powershell
cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\VC\Auxiliary\Build\vcvarsall.bat x64 && cmake -S . -B build-sdl-rel -DHASCIICAM_ENABLE_TESTS=ON && ctest --output-on-failure --test-dir build-sdl-rel -L cli"
```

Run one bounded stdout smoke manually:

```powershell
.\build-sdl-rel\hasciicam.exe -d synthetic:// --frames 2 -O stdout
```

## Windows (camera + SDL)

Build in MSVC environment:

```powershell
cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\VC\Auxiliary\Build\vcvarsall.bat x64 && cmake --build build-sdl-rel --target hasciicam"
```

Run first camera with SDL:

```powershell
.\build-sdl-rel\hasciicam.exe -d "" -O SDL
```

Checks:

- window opens
- live ASCII updates
- closing window exits process

## Windows (size negotiation)

Nearest-size pixel request (Media Foundation path):

```powershell
.\build-sdl-rel\hasciicam.exe -d "" --pixel-size 641x359 -m text -o C:\temp\hasciicam-size.txt
```

Checks:

- startup logs show requested pixel size
- startup logs show negotiated capture size
- startup logs show final ASCII size

DirectShow fallback size check (disable MF at configure time):

```powershell
cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\VC\Auxiliary\Build\vcvarsall.bat x64 && cmake -S . -B build-dshow -G Ninja -DHASCIICAM_ENABLE_CAPTURE_MF=OFF -DHASCIICAM_ENABLE_CAPTURE_DSHOW=ON && cmake --build build-dshow"
.\build-dshow\hasciicam.exe -d "" --pixel-size 641x359 -m text -o C:\temp\hasciicam-dshow-size.txt
```

Checks:

- DirectShow backend opens
- capture starts with nearest available size

## Windows (HTML output)

```powershell
.\build-sdl-rel\hasciicam.exe -d "" -m html -o C:\temp\hasciicam.html
```

Checks:

- output file appears
- refresh updates the page every few seconds
- output is not blank while camera is active

## Linux (V4L2)

```sh
./hasciicam -d /dev/video0 -O SDL
```

Checks:

- V4L2 backend selected
- live ASCII updates

## Linux (virtual camera output)

Prepare a loopback device first:

```sh
sudo modprobe v4l2loopback video_nr=10 card_label="HasciiCam" exclusive_caps=1
v4l2-ctl --device=/dev/video10 --all
```

Run HasciiCam against the existing output node:

```sh
./hasciicam -d synthetic:// --frames 2 -O SDL --virtual-camera --virtual-camera-device /dev/video10
```

Checks:

- startup succeeds without trying to load kernel modules
- `v4l2-ctl` reports the loopback device
- live SDL output stays responsive while the loopback node is attached
- a consumer application can open `/dev/video10` after the producer starts

## Windows (installer)

Build the installer from a Release Windows build tree:

```powershell
powershell -ExecutionPolicy Bypass -File packaging/windows/build-installer.ps1 -BuildDir build
```

Checks:

- an installer appears in `releases/`
- the installer opens with `Full` selected by default
- the `Application only` type omits the virtual-camera component
- a Full install reports `hasciicam_vcamctl status`
- uninstall removes the PATH entry and the virtual-camera registration

## Linux (size negotiation)

```sh
./hasciicam -d /dev/video0 --pixel-size 641x359 -m text -o hasciicam-size.txt
```

Checks:

- logs show requested size and nearest supported/negotiated capture size
- output file is non-empty and updates

## macOS (AVFoundation)

```sh
./hasciicam -d "" -O SDL
```

Checks:

- AVFoundation backend selected
- camera permission prompt appears first run
- live ASCII updates

## Text output

```sh
./hasciicam -m text -o hasciicam.txt
```

Checks:

- `hasciicam.txt` updates
- file contains non-empty ASCII frame text

## HTML output

```sh
./hasciicam -m html -o hasciicam.html
```

Checks:

- `hasciicam.html` updates from temporary publish path
- refresh tag is present
- content is non-empty

## Opt-in visual and camera CTest

Enable visual SDL smoke (`visual_sdl`):

```powershell
cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\VC\Auxiliary\Build\vcvarsall.bat x64 && cmake -S . -B build-sdl-rel -DHASCIICAM_ENABLE_TESTS=ON -DHASCIICAM_ENABLE_VISUAL_TESTS=ON && ctest --output-on-failure --test-dir build-sdl-rel -R visual_sdl"
```

Enable camera smoke (`camera_text`) with explicit selector:

```powershell
cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\VC\Auxiliary\Build\vcvarsall.bat x64 && cmake -S . -B build-sdl-rel -DHASCIICAM_ENABLE_TESTS=ON -DHASCIICAM_ENABLE_CAMERA_TESTS=ON -DHASCIICAM_TEST_CAMERA_DEVICE=<device> && ctest --output-on-failure --test-dir build-sdl-rel -R camera"
```
