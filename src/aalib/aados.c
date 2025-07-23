#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <libtext.h>
#include <go32.h>
#include <dpmi.h>
#include <sys/nearptr.h>
#include <malloc.h>
#include "aalib.h"
#include "aaint.h"
int __use_nearptr_hack;
struct aa_driver dos_d;
static int mode;
static int direction;
#define addmode(mode,x,y)  { \
   nmodes++; \
   lastmode=mode; \
   if(width) { \
   if(abs(p->width-x)+abs(p->height-y)<xd) bestmode=mode,xd=abs(p->width-x)+abs(p->height-y);\
   } \
   if(abs(recwidth-x)+abs(recheight-y)<xd1) recmode=mode,xd1=abs(recwidth-x)+abs(recheight-y);\
   }

static int dos_init(__AA_CONST struct aa_hardware_params *p,__AA_CONST  void *none, struct aa_hardware_params *dest, void **params)
{
    int i, x, y, nmodes = 0;
    int bestmode = -1;
    int recmode = -1;
    int lastmode = 0;
    int noprint = 1;
    int width = 0, height = 0;
    int recwidth = 80, recheight = 50;
    int xd = 1000;
    int xd1 = 1000;
    static aa_font font;
    static char *data,*fdata;
    static struct aa_hardware_params def = {
     NULL, AA_DIM_MASK | AA_REVERSE_MASK | AA_NORMAL_MASK | AA_BOLD_MASK | AA_ALL,
     40, 10,
     137, 60,
     80, 50,
     290, 215,
    };
    char modes[60];
    *dest=def;
    if (p->width || p->height) {
	if (p->width)
	    width = p->width;
	else
	    width = 80;
	if (p->height)
	    width = p->height;
	else
	    height = 80;
    }
    if (p->recwidth)
	recwidth = p->recwidth;
    if (p->recheight)
	recwidth = p->recheight;
    memset(modes, 0, 53);
    TxInit();
  again:;
    if (!noprint)
	printf("Please select internal resolution\n\n");
    if (!noprint)
	printf("VGA modes\n");
    for (i = 0;; i++) {
	if (!noprint && i == 45)
	    printf("\n\nVESA modes\n");
	TxGetModeInfo(i, &x, &y);
	if (x < 0)
	    break;
	if (x) {
	    if (aa_validmode(x, y, p)) {
		if (!noprint)
		    printf("%2i (= %03ix%03i),\t", i, x * 2, y * 2);
		modes[i] = 1;
		addmode(i, x, y);
	    }
	}
    }
    if (aa_validmode(80, 25, p)) {
	if (!noprint)
	    printf("\n\n50 - MDA\t51 - Dual monitor mode\t");
	modes[50] = 1,
	    modes[51] = 1;
	addmode(50, 80, 25);
	addmode(51, 80, 25);
    }
    if (aa_validmode(160, 25, p)) {
	if (!noprint)
	    printf("52 - Dual monitor mode plus");
	modes[52] = 1;
	addmode(51, 160, 25);
    }
    if (!noprint)
	printf("\n");
    mode = 1;
    if (!xd)
	mode = bestmode;
    else {
	if (nmodes == 1)
	    mode = lastmode;
	else {
	    if (noprint) {
		if (!nmodes)
		    return 0;
		noprint = 0;
		goto again;
	    }
	    printf("\b\b\nSelect resolution (press enter to set default resolution): ");
	    do {
		char c[256];
		gets(c);
		mode = -1;
		sscanf(c, "%i", &mode);
		if (mode == -1) {
		    if (recmode >= 0)
			mode = recmode;
		    else
			mode = 11;
		    break;
		}
	    } while (mode < 0 || mode >= 53 || !modes[mode]);
	}
    }
    if (mode == 52) {
	printf("1-MDA is at the left\t2-MDA is at the right\n");
	do {
	    scanf("%i", &direction);
	} while (direction < 1 || direction > 2);
    }
    if (mode < 50) {
	if (TxSetMode(mode) == -1) {
	    printf("Inicialization failed\n");
	    return 0;
	}
	font.height = GetFontHeight();
	font.data = fdata = malloc(font.height * 256);
	font.name = "Font used by your vgacard";
	font.shortname = "current";
	data = malloc(0x10000);
	TxGetFont(data);
	for (i = 0; i < 256; i++) {
	    int y;
	    for (y = 0; y < font.height; y++)
		fdata[i * font.height + y] = data[i * 32 + y];
	}
	free(data);
	aa_registerfont(&font);
	dest->font = &font;
    } else {
	dest->font = &aa_font14;
	dest->supported = AA_NORMAL_MASK | AA_REVERSE_MASK | AA_BOLD_MASK | AA_EXTENDED;
	if (mode > 50)
	    TxSetMode(7);
    }
    if (mode == 52)
	dest->mmwidth = 290 * 2;
    aa_recommendlowkbd("dos");
    return 1;
}
static void dos_uninit(aa_context * c)
{
    if (mode != 50)
	TxUninit();
}
static void dos_getsize(aa_context * c, int *width, int *height)
{
    if (mode < 50) {
	*width = /*info.screenwidtah */ TxMaxx() + 1;
	*height = /*info.screenheight */ TxMaxy() + 1;
    } else {
	*width = 80;
	*height = 25;
	if (mode == 52)
	    (*width) *= 2;
    }
}

static void mymovedata(char *c1, int d, int l)
{
    if (!__use_nearptr_hack) {
	movedata(_my_ds(), (int) c1, _dos_ds, d, l);
    } else {
/*__djgpp_nearptr_enable();*/
	memcpy(__djgpp_conventional_base + (char *) d, c1, l);
/*__djgpp_nearptr_disable();*/

    }
}

static void dos_flush(aa_context * c)
{				/*ugly..but better than previous one :) */
    int x, end = aa_scrwidth(c) * aa_scrheight(c);
    char buffer[aa_scrwidth(c) * aa_scrheight(c) * 2], *pos;
    unsigned char data[] =
    {0x07, 0x08, 0x0f, 0x0f, 0x70, 0x17};
    /*pos=__djgpp_conventional_base+0xB8000; */
    pos = buffer;
    for (x = 0; x < end; x++) {
	*(pos++) = c->textbuffer[x];
	/*if (c->attrbuffer[x] < 7) */
	*(pos++) = data[c->attrbuffer[x]];
	/*else
	   *(pos++)=0x27; */
    }
    if (mode == 52) {
	for (x = 0; x < end; x += 160) {
	    if (direction == 2) {
		mymovedata(&buffer[x * 2], 0xb8000 + x, 160),
		    mymovedata(&buffer[x * 2 + 160], 0xb0000 + x, 160);
	    } else {
		mymovedata(&buffer[x * 2], 0xb0000 + x, 160),
		    mymovedata(&buffer[x * 2 + 160], 0xb8000 + x, 160);
	    }
	}
    } else {
	if (mode != 50)
	    mymovedata(&buffer[0], 0xb8000, aa_scrwidth(c) * aa_scrheight(c) * 2);
	if (mode >= 50)
	    mymovedata(&buffer[0], 0xb0000, aa_scrwidth(c) * aa_scrheight(c) * 2);
    }
    /*dosmemput(buffer,aa_scrwidth(c)*aa_scrheight(c)*2,0xB8000); */
}

static void mono_gotoxy(int x, int y)
{
	unsigned short addr=y*TxScreenSizex()+x;
	asm ("
		movw	%0,%%bx
		movb	%%bh,%%ah
		movb	$0x0e,%%al
		movw	$0x3b4,%%dx
		outw	%%ax,%%dx
		incb	%%al
		movb	%%bl,%%ah
		outw	%%ax,%%dx
		"
		:
		:"r"(addr)
		:"%eax","%ebx","%edx"
	);
}

static void dos_gotoxy(aa_context * c, int x, int y)
{
    if (mode == 52) {
      if(x<80) {
        if(direction==2) TxGotoxy(x,y),mono_gotoxy(255,255);
        if(direction==1) TxGotoxy(255,255),mono_gotoxy(x,y);
      } else {
        if(direction==1) TxGotoxy(x-80,y),mono_gotoxy(255,255);
        if(direction==2) TxGotoxy(255,255),mono_gotoxy(x-80,y);
      }
    } else {
    if (mode != 50)
	TxGotoxy(x, y);
    if (mode>=50) mono_gotoxy(x,y);
    }
}

static void dos_cursor(aa_context *c,int mode1)
{
   if(!mode1) {
   if(mode!=50) TxGotoxy(255,255);
   if(mode>=50) mono_gotoxy(255,255);
   } else dos_gotoxy(c,c->cursorx,c->cursory);
#if 0
   if(!mode) {
    TxSetCursor(22,24);
   } else {
    TxSetCursor(1,0);
   }
#endif
}
__AA_CONST struct aa_driver dos_d =
{
    "dos", "dos pc driver 1.0",
    dos_init,
    dos_uninit,
    dos_getsize,
    NULL,
    NULL,
    dos_gotoxy,
    dos_flush,
    dos_cursor
};
