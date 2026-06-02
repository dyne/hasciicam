#include "aalib.h"
#include "aaint.h"
#undef aa_currentfont
__AA_CONST struct aa_font *
aa_currentfont (aa_context *a)
{
  return a->params.font;
}
