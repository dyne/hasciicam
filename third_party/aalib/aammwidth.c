#include "aalib.h"
#include "aaint.h"
#undef aa_mmwidth
int 
aa_mmwidth (aa_context *a)
{
  return a->params.mmwidth;
}
