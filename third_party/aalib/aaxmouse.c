#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include "config.h"
#ifdef X11_MOUSEDRIVER
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include "aalib.h"
#include "aaint.h"
#include "aaxint.h"

static int X_init(aa_context * c, int mode)
{
    struct xdriverdata *d=c->driverdata;
    if (c->driver!=&X11_d)
	return 0;
    d->attr.event_mask |= ButtonPressMask | ButtonReleaseMask |
	(mode & AA_MOUSEMOVEMASK ? PointerMotionMask : 0);
    XSelectInput(d->dp, d->wi, d->attr.event_mask);
    return 1;
}
static void X_uninit(aa_context * c)
{
    struct xdriverdata *d=c->driverdata;
    d->attr.event_mask &= ~(ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask);
    XSelectInput(d->dp, d->wi, d->attr.event_mask);
}

int __X_mousex, __X_mousey, __X_buttons;
static void X_getmouse(aa_context * c, int *x, int *y, int *b)
{
    struct xdriverdata *d=c->driverdata;
    *x = __X_mousex / d->fontwidth;
    *y = __X_mousey / d->fontheight;
    *b = 0;
    if (__X_buttons & Button1Mask)
	*b |= AA_BUTTON1;
    if (__X_buttons & Button2Mask)
	*b |= AA_BUTTON2;
    if (__X_buttons & Button3Mask)
	*b |= AA_BUTTON3;
}
__AA_CONST struct aa_mousedriver mouse_X11_d =
{
    "X11", "X11 mouse driver 1.0",
    AA_MOUSEALLMASK,
    X_init,
    X_uninit,
    X_getmouse,
    NULL
};
#endif
