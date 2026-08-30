# Hasciicam Usage Guide

This software is operated from a terminal and invites you to enjoy the
aesthetics of it `:^)`.

To see a brief list of command line options:

```sh
./hasciicam -h
```

## Still-image input

Use `--image picture.png` or `--image picture.jpg` to repeat a still image as
the capture source. It works with live, text, and HTML output.

```
hasciicam 2.6.0 - (h)ascii 4 the masses! - https://ascii.dyne.org
(c)2000-2026 RASTASOFT by Jaromil @ Dyne.org

Usage: hasciicam [options] [rendering options] [aalib options]
options:
 -h --help         this help
 -H --aahelp       aalib complete help
 -v --version      version information
 -q --quiet        be quiet
 -m --mode         mode: live|html|text      - default live
 -d --device       video grabbing device     - default /dev/video
 -i --input        input channel number      - default 1
 -s --size         contextual size WxH       - html chars, else window px
    --pixel-size   output window size WxH    - preferred final live pixels
    --char-size    output char size WxH      - preferred ascii grid
 -o --aafile       dumped file               - default hasciicam.[txt|html]
 -O --aadriver     aalib driver: X11|curses|SDL|stdout - default auto
    --sdl-renderer accelerated|software|auto - SDL renderer choice
    --sdl-vsync    on|off|auto               - SDL presentation sync
    --fullscreen   start SDL live output fullscreen
    --mirror       x|-x|y|-y                 - flip image, default x
 -D --daemon       run in background         - default foregrond
    --frames N     stop after N rendered frames (test/smoke)
 -U --uid          setuid (int)              - default current
 -G --gid          setgid (int)              - default current
rendering options:
 -S --font-size    html font size (1-4)      - default 1
 -a --font-face    html font to use          - default courier
 -r --refresh      refresh delay             - default 2
 -b --aabright     ascii brightness          - default 60
 -c --aacontrast   ascii contrast            - default 4
 -g --aagamma      ascii gamma               - default 3
    --aa-dimmer    on|off                    - default on
 -I --invert       invert colors             - default off
 -B --background   background color (hex)    - default 000000
 -F --foreground   foreground color (hex)    - default FFFFFF
```

## Live text-frame snapshot

In SDL live mode, open the right-click controls and use **Output → Save text
frame**. The default destination is `hasciicam.txt`; edit the path before
saving when needed. This is a one-shot export of the next fully rendered grid,
so current brightness, contrast, gamma, inversion, mirror, and font changes
are reflected in its characters. The text file has no colors or AA attributes.

This differs from `-m text -o FILE`, which continuously overwrites `FILE` with
new frames instead of taking a live GUI snapshot.

## Linux Virtual Camera Output

HasciiCam can write its live SDL ASCII frames into an existing
[`v4l2loopback`](https://github.com/v4l2loopback/v4l2loopback) output device.
HasciiCam does not load the kernel module, create devices, or change system
permissions for you.

Typical setup on Linux:

```sh
sudo modprobe v4l2loopback video_nr=10 card_label="HasciiCam" exclusive_caps=1
v4l2-ctl --device=/dev/video10 --all
./hasciicam -d synthetic:// -O SDL --virtual-camera --virtual-camera-device /dev/video10
```

Notes:

- `exclusive_caps=1` is the common setting for browser and WebRTC consumers.
- `v4l2-ctl` is useful for verification and troubleshooting.
- HasciiCam negotiates the output format itself; you do not need to call
  `v4l2loopback-ctl set-fps` or `set-caps` for normal use.
- The loopback device must already exist and be writable by the current user.

## Build From Source

For instructions on how to build see the [Source Code](https://github.com/dyne/hasciicam).

## License

Hasciicam is Copyright (C) 2001-2026 by the Dyne.org foundation.

This source code is free software; you can redistribute it and/or modify it under
the terms of the GNU Public License as published by the Free Software Foundation,
either version 2 of the License, or at your option any later version.

This source code is distributed in the hope that it will be useful, but without
any warranty; without even the implied warranty of merchantability or fitness for
a particular purpose. Refer to the GNU Public License for more details.
