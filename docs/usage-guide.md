# Hasciicam Usage Guide

Hasciicam makes it possible to have live ASCII video on the web.

![Jaromil in the Hascii spliff screenshot](img/jaro_hasciicam.jpg)

Hasciicam captures video from a TV card and renders it into ASCII. It can format
the output as an HTML page with a refresh tag, as a live ASCII window, or as a
plain text file. This lets a GNU/Linux box with a video device and a modest
connection publish a live ASCII video feed that can be browsed without plugins,
Java, or other client-side requirements.

- [HTML screenshots featuring chmod and thing.net](hasciicam001.html)
- [Portrait of Wolfgang Staehle, 2001](hasciicam008.html)
- [Portrait of Richard M. Stallman, 2002](rms-hasciicam.html)

Swiss artist installation using Hasciicam and printers, 2015:

![Swiss artist installation using Hasciicam and printers](img/swiss_installation_printers.jpg)

## Installation

On Debian and Ubuntu, Hasciicam packages are ready to install:

```sh
sudo apt-get install hasciicam
```

On Red Hat and derived distributions, a package may be available too.

## Usage

This software is operated from a terminal and invites you to enjoy the
aesthetics of it `:^)`.

To see a brief list of command line options:

```sh
hasciicam -h
```

To see the manual:

```sh
man hasciicam
```

The generated manual page is also available as [HTML](manpage.html).

## Build From Source

If you want to compile Hasciicam from source, you need `aalib`, the library that
makes ASCII rendering possible. If your distribution does not include it, fetch
it from the Hasciicam web page.

To compile the source code:

```sh
autoreconf -i && ./configure && make
```

To install it:

```sh
make install
```

People have reported success with several PCI and USB devices. Refer to
Video4Linux documentation for more information.

### Internals

Hasciicam is written in plain C and operated from the command line. It comes with
a comfortable help screen through the `-h` option and a [manual page](manpage.html).

Hasciicam grabs video using the Video4Linux2 API. It captures YUV420, uses the
luminance component to obtain a grayscale frame, renders each frame into ASCII
through the AA-lib engine, and wraps the result in HTML with a refresh tag.

FTP push technology is supported to publish a Hascii feed on an online web
server. This is implemented with simple C code that wraps execution of a Unix
FTP client.

Hasciicam should be portable to operating systems beyond GNU/Linux, but using it
on free GNU systems remains strongly advised.

## Credits

Hasciicam is a [RASTASOFT](https://rastasoft.org) production by
[Denis "Jaromil" Roio](https://jaromil.dyne.org).

People who contributed to the Hasciicam project:

- Jan Hubicka and the [AA-project](http://aa-project.sourceforge.net/) crew for
  the ASCII rendering library
- Gerd Knorr, whose webcam source code inspired the grab code
- Mathop, also known as Josto, for CSS help
- August Black for Iomegabuz hacks
- Boffh for USB camera hacks
- Martin Guy for buffer overflow prevention
- Rat for text dump support
- PBM and Megabug for watching ASCII horizons
- Rapid for security and bug fixes
- Alessandro Preite Martinez for SGI Irix support in 0.9
- Thomas Pfau for the FTP library
- Blended for wider webcam support
- Dan Stowell for Video4Linux2 API support

Special thanks to LOA hacklab Milano, Hell Voyager, Acme, Rasty, Martinez,
servus.at, maddler.net, flyinglinux.net, autistici.org, and FREAKNET medialab
Catania.

## License

Hasciicam is Copyright (C) 2001-2019 by the Dyne.org foundation.

This source code is free software; you can redistribute it and/or modify it under
the terms of the GNU Public License as published by the Free Software Foundation,
either version 2 of the License, or at your option any later version.

This source code is distributed in the hope that it will be useful, but without
any warranty; without even the implied warranty of merchantability or fitness for
a particular purpose. Refer to the GNU Public License for more details.
