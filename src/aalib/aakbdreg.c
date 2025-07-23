#include <malloc.h>
#include <string.h>

// #include "config.h"
#include "aalib.h"
#include "aaint.h"
__AA_CONST struct aa_kbddriver * __AA_CONST aa_kbddrivers[] =
{
#ifdef LINUX_KBDDRIVER
    &kbd_linux_d,
#endif
#ifdef DJGPP
    &kbd_dos_d,
#else
#ifdef SDL_KBDDRIVER
    &kbd_SDL_d,
#endif
#ifdef X11_KBDDRIVER
    &kbd_X11_d,
#endif
#ifdef SLANG_KBDDRIVER
    &kbd_slang_d,
#endif
#ifdef CURSES_KBDDRIVER
    &kbd_curses_d,
#endif
#ifdef OS2_KBDDRIVER
    &kbd_os2_d,
#endif
#endif
    &kbd_stdin_d,
    NULL
};
int aa_autoinitkbd(struct aa_context *context, int mode)
{
    int i = 0;
    int ok = 0;
    char *t;
    while ((t = aa_getfirst(&aa_kbdrecommended)) != NULL) {
	if (!ok) {
	    for (i = 0; aa_kbddrivers[i] != NULL; i++) {
		if (!strcmp(t, aa_kbddrivers[i]->name) || !strcmp(t, aa_kbddrivers[i]->shortname)) {
		    ok = aa_initkbd(context, aa_kbddrivers[i], mode);
		    break;
		}
	    }
	    if (aa_kbddrivers[i] == NULL)
		printf("Driver %s unknown", t);
	    free(t);
	}
    }
    i = 0;
    if (!ok)
	while (aa_kbddrivers[i] != NULL) {
	    if (aa_initkbd(context, aa_kbddrivers[i], mode)) {
		return 1;
	    }
	    i++;
	}
    return (ok);
}
