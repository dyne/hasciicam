#include <stdlib.h>

#include "aalib.h"
#include "aaint.h"
int main(int argc, char **argv)
{
    aa_context *c;
    char str[256];
    char str1[256];
    char str2[256];
    if (!aa_parseoptions(NULL, NULL, &argc, argv) || argc != 1) {
	printf("%s", aa_help);
	exit(1);
    }
    c = aa_autoinit(&aa_defparams);
    if (c == NULL) {
	printf("aalib initialization failed\n");
	exit(1);
    }
    aa_autoinitkbd(c, 0);
    aa_autoinitmouse(c, AA_MOUSEALLMASK);
    sprintf(str, "\n Current driver:%s\n"
	    "  Short name   :%s\n"
	    " Font          :%s\n"
	    "  Short name   :%s\n"
	    "  height       :%i\n"
	    " Width         :%i\n"
	    " Height        :%i\n"
	    " Width in mm   :%i\n"
	    " Height in mm  :%i\n"
	    " Supported     :%i\n",
	    c->driver->name,
	    c->driver->shortname,
	    c->params.font->name,
	    c->params.font->shortname,
	    c->params.font->height,
	    c->params.width,
	    c->params.height,
	    c->params.mmwidth,
	    c->params.mmheight,
	    c->driverparams.supported);
    if (c->kbddriver != NULL)
	sprintf(str1, "\n Current driver:%s\n"
		"  Short name   :%s\n"
		" Flags         :%i\n",
		c->kbddriver->name,
		c->kbddriver->shortname,
		c->kbddriver->flags);
    else
	sprintf(str1, "not available");
    if (c->mousedriver != NULL)
	sprintf(str2, "\n Current driver:%s\n"
		"  Short name   :%s\n"
		" Flags         :%i\n",
		c->mousedriver->name,
		c->mousedriver->shortname,
		c->mousedriver->flags);
    else
	sprintf(str2, "not available");
    aa_close(c);
    printf("AAlib version:%i.%i\nDisplay:%s\nKeyboard:%s\nMouse:%s\n", AA_LIB_VERSION, AA_LIB_MINNOR, str, str1, str2);
    return (0);
}
