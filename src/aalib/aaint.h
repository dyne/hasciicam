#ifndef __AAINT_INCLUDED__
#define __AAINT_INCLUDED__
#include <stdio.h>

/* Macros used for calculating fonts sizes.  */
#define NCHARS (256*AA_NATTRS)

/* Mode validation.  */
#define aa_validmode(x,y,params)  \
      ((((params)->minwidth||(params)->maxwidth)||((params)->width==(x)||!(params)->width))&& \
      (((params)->minheight||(params)->maxheight)||((params)->height==(y)||!(params)->height)) && \
      ((params)->minwidth?(params->minwidth)<=(x):1)&& \
      ((params)->minheight?(params->minheight)<=(x):1)&& \
      ((params)->maxwidth?(params->maxwidth)>=(x):1)&& \
      ((params)->maxheight?(params->maxheight)>=(x):1))

#define ALOWED(i,s) ((isgraph((i)&0xff)||(((i)&0xff)==' ')||(((i)&0xff)>160&&(s&AA_EIGHT))||((s&AA_ALL)&&((i)&0xff)))&&(s&TOMASK(((i)>>8))))
#define ALOWED1(i,s) (1)
#define TOMASK(i) (1<<(i))



struct aa_edit {
    int maxsize;
    char *data;
    int cursor;
    int clearafterpress;
    int printpos;
    int x, y, size;
    struct aa_context *c;
};

struct parameters {
    unsigned int p[AA_NPARAMS];
};

struct aa_linkedlist {
    char *text;
    struct aa_linkedlist *next, *previous;
};
struct aa_graphics {
    char *videoram;
    int width;
    int height;
    struct aa_font *font;
    char *cache;
    char *attrcache;
    int bgcolor;
    int fgcolor;
    int dimcolor;
    int boldcolor;
    int specialcolor;
    char charset[AA_NATTRS];
};

void __aa_calcparams(__AA_CONST struct aa_font *font,
		     struct parameters *parameters,
		     int supported,double dimmul, double boldmul);

#endif
