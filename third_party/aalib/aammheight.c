#include "aalib.h"
#include "aaint.h"
#undef aa_mmheight
int 
aa_mmheight (aa_context *a)
{
  return a->params.mmheight;
}
