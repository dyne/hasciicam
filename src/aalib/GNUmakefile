SOURCES := aafont.o aafonts.o aavyhen.o aamktabl.o aamem.o aaprintf.o	\
aahelp.o aaslnkbd.o aafastre.o aalinux.o aalinuxkbd.o aax.o	\
aaxkbd.o aaxmouse.o fontgl.o font16.o font14.o fontx16.o fontx13.o	\
fontx13b.o fontcour.o aarec.o font9.o font8.o fontline.o aacurses.o	\
aacurkbd.o aacurmou.o aalib.o aaflush.o aarender.o aaout.o aain.o	\
aaedit.o aaregist.o aakbdreg.o aagpm.o aamoureg.o aasave.o aastdout.o	\
aastdin.o aaparse.o aaimgwidth.o aaimgheight.o aascrwidth.o		\
aascrheight.o aammwidth.o aammheight.o aaimage.o aatext.o aacurrfnt.o	\
aaattrs.o aaputpixel.o aarecfunc.o aasdl.o

CFLAGS ?= -O0 -g -fsanitize=address -fsanitize=undefined
SDL_CFLAGS ?= $(shell sdl2-config --cflags 2>/dev/null || echo "")
SDL_LIBS ?= $(shell sdl2-config --libs 2>/dev/null || echo "")

all: $(SOURCES)
	${AR} -r libaa.a $(SOURCES)

.c.o:
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@

aasdl.o: aasdl.c
	$(CC) \
	$(CFLAGS) $(SDL_CFLAGS) \
	-c $< -o $@ \
	-DSDL_DRIVER

aax.o: aax.c
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DX11_DRIVER

aacurses.o: aacurses.c
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DCURSES_DRIVER

aaslang.o: aaslang.c
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DSLANG_DRIVER

aaregist.o: aaregist.c
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DSDL_DRIVER -DX11_DRIVER -DCURSES_DRIVER

clean:
	rm -f *.o *.a
