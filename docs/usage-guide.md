# Hasciicam Usage Guide

This software is operated from a terminal and invites you to enjoy the
aesthetics of it `:^)`.

To see a brief list of command line options:

```sh
./hasciicam -h
```


```
hasciicam 2.0 - (h)ascii 4 the masses! - https://ascii.dyne.org
(c)2000-2025 RASTASOFT by Jaromil @ Dyne.org

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
 -I --invert       invert colors             - default off
 -B --background   background color (hex)    - default 000000
 -F --foreground   foreground color (hex)    - default 00FF00
 ```

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
