#include "aalib.h"
#include "aaint.h"
#undef aa_scrwidth
int 
aa_scrwidth (aa_context *a)
{
  return a->params.width;
}
