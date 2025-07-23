#include "aalib.h"
#include "aaint.h"
#undef aa_text
char *
aa_text (aa_context *a)
{
  return (char *)a->textbuffer;
}
