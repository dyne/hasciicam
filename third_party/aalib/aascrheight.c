#include "aalib.h"
#include "aaint.h"
#undef aa_scrheight
int 
aa_scrheight (aa_context *a)
{
  return a->params.height;
}
