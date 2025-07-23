#include <stdio.h>
// #include "config.h"
#include "aalib.h"
#include "aaint.h"
#define MAXFONTS 246
__AA_CONST struct aa_font *aa_fonts[MAXFONTS + 1] =
{
#ifdef VYHEN_SUPPORT
    &aa_fontvyhen,
#endif
    &aa_font8, &aa_font9, &aa_font14, &aa_font16, &aa_fontX13, &aa_fontX13B, &aa_fontX16, &aa_fontline, &aa_fontgl, &aa_fontcourier, 
    NULL
};

int aa_registerfont(__AA_CONST struct aa_font *f)
{
    int i;
    for (i = 0; i < MAXFONTS && aa_fonts[i] != NULL; i++);
    if (i == MAXFONTS)
	return 0;
    aa_fonts[i] = f;
    aa_fonts[i + 1] = 0;
    return (1);
}
