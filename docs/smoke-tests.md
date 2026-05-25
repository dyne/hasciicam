# Smoke Tests

This file collects short manual checks for supported host paths.

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
