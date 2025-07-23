#include "aalib.h"
#include "aaint.h"

#undef aa_recommendhikbd
#undef aa_recommendhimouse
#undef aa_recommendhidisplay
#undef aa_recommendlowkbd
#undef aa_recommendlowmouse
#undef aa_recommendlowdisplay
void
aa_recommendhikbd (__AA_CONST char *t)
{
  aa_recommendhi (&aa_kbdrecommended, t);
}
void
aa_recommendhimouse (__AA_CONST char *t)
{
  aa_recommendhi (&aa_mouserecommended, t);
}
void
aa_recommendhidisplay (__AA_CONST char *t)
{
  aa_recommendhi (&aa_displayrecommended, t);
}
void
aa_recommendlowkbd (__AA_CONST char *t)
{
  aa_recommendlow (&aa_kbdrecommended, t);
}
void
aa_recommendlowmouse (__AA_CONST char *t)
{
  aa_recommendlow (&aa_mouserecommended, t);
}
void
aa_recommendlowdisplay (__AA_CONST char *t)
{
  aa_recommendlow (&aa_displayrecommended, t);
}
