PACKAGE := hasciicam
VERSION ?= 2.0.0

all: src/hasciicam.o src/aalib/libaa.a
	$(CC) -o hasciicam src/hasciicam.o \
	src/aalib/libaa.a -L/usr/lib/x86_64-linux-gnu/ -lSDL2 -lSDL2_ttf -lm

src/aalib/libaa.a:
	CC=$(CC) CFLAGS="$(CFLAGS) -DSDL_DRIVER" $(MAKE) -C src/aalib

clean:
	rm -f hasciicam src/hasciicam.o
	$(MAKE) clean -C src/aalib

.c.o:
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DPREFIX=\"${PREFIX}\" \
	-DPACKAGE=\"${PACKAGE}\" \
	-DVERSION=\"${VERSION}\" \
	-DCURRENT_YEAR=\"${CURRENT_YEAR}\"


# static: CC := /opt/musl-dyne/bin/x86_64-linux-musl-cc
# static: LD := /opt/musl-dyne/bin/x86_64-linux-musl-ld
# static: AR := /opt/musl-dyne/bin/x86_64-linux-musl-ar
# static:
# 	CC=$(CC) LD=$(LD) AR=$(AR) \
# 		$(MAKE) -C src
