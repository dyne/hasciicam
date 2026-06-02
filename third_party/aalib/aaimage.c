#include "aalib.h"
#define NOFAST
#include "aaint.h"
#undef aa_image
char *
aa_image (aa_context *a)
{
  return (char *)a->imagebuffer;
}
