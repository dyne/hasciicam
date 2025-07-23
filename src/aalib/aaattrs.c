#include "aalib.h"
#include "aaint.h"
#undef aa_attrs
char *
aa_attrs (aa_context *a)
{
  return (char *)a->attrbuffer;
}
