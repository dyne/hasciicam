SOURCES := aafont.o aafonts.o aavyhen.o aamktabl.o aamem.o aaprintf.o	\
aahelp.o aaslnkbd.o aaslang.o aafastre.o aalinux.o aalinuxkbd.o aax.o	\
aaxkbd.o aaxmouse.o fontgl.o font16.o font14.o fontx16.o fontx13.o		\
fontx13b.o fontcour.o aarec.o font9.o font8.o fontline.o aacurses.o		\
aacurkbd.o aacurmou.o aalib.o aaflush.o aarender.o aaout.o aain.o			\
aaedit.o aaregist.o aakbdreg.o aagpm.o aamoureg.o aasave.o aastdout.o	\
aastdin.o aaparse.o aaimgwidth.o aaimgheight.o aascrwidth.o						\
aascrheight.o aammwidth.o aammheight.o aaimage.o aatext.o aacurrfnt.o	\
aaattrs.o aaputpixel.o aarecfunc.o

CFLAGS ?= -Os

all: $(SOURCES)
	${AR} -r libaa.a $(SOURCES)

.c.o:
	$(CC) \
	$(CFLAGS) \
	-c $< -o $@ \
	-DPREFIX=\"${PREFIX}\" \
	-DVERSION=\"${VERSION}\" \
	-DCURRENT_YEAR=\"${CURRENT_YEAR}\"

clean:
	rm -f *.o *.a
