#include "aalib.h"
#include "aaint.h"
#undef aa_imgwidth
int 
aa_imgwidth (aa_context *a)
{
  return a->imgwidth;
}
