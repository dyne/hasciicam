#include <malloc.h>
#include <string.h>

// #include "config.h"
#include "aalib.h"
#include "aaint.h"
__AA_CONST struct aa_mousedriver * __AA_CONST aa_mousedrivers[] =
{
#ifdef DJGPP
    &mouse_dos_d,
#else
#ifdef X11_MOUSEDRIVER
    &mouse_X11_d,
#endif
#ifdef GPM_MOUSEDRIVER
    &mouse_gpm_d,
#endif
#ifdef CURSES_MOUSEDRIVER
    &mouse_curses_d,
#endif
#ifdef OS2_MOUSEDRIVER
    &mouse_os2_d,
#endif
#endif
    NULL
};
int aa_autoinitmouse(struct aa_context *context, int mode)
{
    int i = 0;
    int ok = 0;
    char *t;
    while ((t = aa_getfirst(&aa_mouserecommended)) != NULL) {
	if (!ok) {
	    for (i = 0; aa_mousedrivers[i] != NULL; i++) {
		if (!strcmp(t, aa_mousedrivers[i]->name) || !strcmp(t, aa_mousedrivers[i]->shortname)) {
		    ok = aa_initmouse(context, aa_mousedrivers[i], mode);
		    break;
		}
	    }
	    if (aa_mousedrivers[i] == NULL)
		printf("Driver %s unknown", t);
	    free(t);
	}
    }
    i = 0;
    if (!ok)
	while (aa_mousedrivers[i] != NULL) {
	    if (aa_initmouse(context, aa_mousedrivers[i], mode)) {
		return 1;
	    }
	    i++;
	}
    return (ok);
}
