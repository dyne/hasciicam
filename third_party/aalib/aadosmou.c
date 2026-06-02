#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <dpmi.h>
#include "aalib.h"
#include "aaint.h"
// #include "config.h"
static int dos_init(struct aa_context *context, int mode)
{
    __dpmi_regs r;
    r.x.ax=0;
    __dpmi_int(0x33,&r);
    if(r.x.ax==0) return 0;
    r.x.ax=7;
    r.x.cx=0;
    r.x.dx=8*aa_scrwidth(context)-8;
    __dpmi_int(0x33,&r);
    r.x.ax=8;
    r.x.cx=0;
    r.x.dx=8*aa_scrheight(context)-8;
    __dpmi_int(0x33,&r);
    r.x.ax=1; 
    __dpmi_int(0x33,&r);
    return 1;
}
static void dos_uninit(aa_context * c)
{
  __dpmi_regs r;
  r.x.ax=2; 
  __dpmi_int(0x33,&r);
}

static void dos_mouse(aa_context * c, int *x, int *y, int *b)
{
    __dpmi_regs r;
    r.x.ax=3;
    __dpmi_int(0x33,&r);
    *x = r.x.cx/8;
    *y = r.x.dx/8;
    *b = 0;
    if (r.x.bx & 1)
	*b |= AA_BUTTON1;
    if (r.x.bx & 4)
	*b |= AA_BUTTON2;
    if (r.x.bx & 2)
	*b |= AA_BUTTON3;
}
static void dos_mousemode(aa_context *c,int m)
{
  __dpmi_regs r;
  if(m)
    r.x.ax=1; else
    r.x.ax=2; 
  __dpmi_int(0x33,&r);
}

__AA_CONST struct aa_mousedriver mouse_dos_d =
{
    "dos", "Dos mouse driver 1.0",
    AA_MOUSEALLMASK | AA_HIDECURSOR,
    dos_init,
    dos_uninit,
    dos_mouse,
    dos_mousemode,
};
