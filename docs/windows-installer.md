# HasciiCam Windows Installer

The Windows installer is built from `packaging/windows/hasciicam.iss` with the
helper script `packaging/windows/build-installer.ps1`.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File packaging/windows/build-installer.ps1 -BuildDir build
```

The script:

1. Runs `cmake --install` into a staging directory.
2. Copies the repository docs and license tree that CMake does not install.
3. Invokes `ISCC.exe` from `C:\Program Files\Inno Setup 7` unless overridden.

## Installer shape

- x64 only
- machine-wide
- `Full` is the default install type
- `Application only` omits the virtual-camera component
- the virtual-camera component is checked by default

The virtual-camera component requires administrator rights and Windows 11
build 22000 or later. It is installed and registered through
`hasciicam_vcamctl.exe`, not `regsvr32`.

## Smoke test

After building the installer, run a manual install in Windows Sandbox or a
disposable VM and verify:

```powershell
.\releases\hasciicam-<version>-windows-x64-setup.exe
```

Then check:

- `hasciicam --version`
- `hasciicam -h`
- `hasciicam_vcamctl status`
- uninstall removes the PATH entry and the virtual-camera registration
