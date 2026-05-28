#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include "config.h"
#ifdef X11_KBDDRIVER
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include "aalib.h"
#include "aaint.h"
#include "aaxint.h"
#define dr (d->pixmapmode?d->pi:d->wi)

static int X_init(aa_context * c, int mode)
{
    struct xdriverdata *d=c->driverdata;
    if(c->driver!=&X11_d) return 0;
    d->attr.event_mask |= StructureNotifyMask | KeyPressMask | (mode & AA_SENDRELEASE ? KeyReleaseMask : 0);
    XSelectInput(d->dp, d->wi, d->attr.event_mask);
    aa_recommendlowmouse("X11");
    return 1;
}
static void X_uninit(aa_context * c)
{
    struct xdriverdata *d=c->driverdata;
    d->attr.event_mask &= ~(KeyPressMask | KeyReleaseMask);
    XSelectInput(d->dp, d->wi, d->attr.event_mask);
}
static int decodekey(XEvent * ev)
{
    KeySym ksym;
    char name[256];
    switch (ksym = XLookupKeysym(&ev->xkey, 0)) {
    case XK_Left:
	return (AA_LEFT);
    case XK_Right:
	return (AA_RIGHT);
    case XK_Up:
	return (AA_UP);
    case XK_Down:
	return (AA_DOWN);
    case XK_Escape:
	return (AA_ESC);
    case XK_space:
	return (' ');
    case XK_Return:
#ifdef XK_cr
    case XK_cr:
#endif
	return (13);
    case XK_BackSpace:
    case XK_Delete:
	return (AA_BACKSPACE);
    }
    name[XLookupString(&ev->xkey,name,256,&ksym,NULL)]=0;
    if(strlen(name)!=1) return (AA_UNKNOWN+ksym);
    return name[0];
}
extern int __X_mousex, __X_mousey, __X_buttons;
static int X_getchar(aa_context * c, int wait)
{
    struct xdriverdata *d=c->driverdata;
    while (1) {
	XEvent ev;
	if (!wait && !XPending(d->dp))
	    return AA_NONE;
	XNextEvent(d->dp, &ev);
	switch (ev.type) {
	case ButtonPress:
	    ev.xbutton.state |= 1 << (ev.xbutton.button + 7);
	    goto skip;
	case ButtonRelease:
	    ev.xbutton.state &= ~(1 << (ev.xbutton.button + 7));
	  skip:;
	case MotionNotify:
	    __X_mousex = ev.xbutton.x;
	    __X_mousey = ev.xbutton.y;
	    __X_buttons = ev.xbutton.state;
	    return (AA_MOUSE);
	case Expose:
	    XSync(d->dp, 0);
            __aa_X_redraw(c);
	    break;
	case ConfigureNotify:
	    if (__aa_X_getsize(c,d))
		return (AA_RESIZE);
	    break;
	case KeyPress:
	    return (decodekey(&ev));
	case KeyRelease:
	    return (decodekey(&ev) | AA_RELEASE);

	}
    }
}

__AA_CONST struct aa_kbddriver kbd_X11_d =
{
    "X11", "X11 keyboard driver 1.0",
    AA_SENDRELEASE,
    X_init,
    X_uninit,
    X_getchar,
};
#endif
