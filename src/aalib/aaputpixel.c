#include <aalib.h>
#include "aaint.h"
#undef aa_putpixel
void
aa_putpixel (aa_context *c, int x, int y, int color)
{
	c->imagebuffer[x+y*aa_imgwidth(c)]=color;
}
