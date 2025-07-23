#include "aalib.h"
#define NOFAST
#include "aaint.h"
#undef aa_imgheight
int 
aa_imgheight (aa_context *a)
{
  return a->imgheight;
}
