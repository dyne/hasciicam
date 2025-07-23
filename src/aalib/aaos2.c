// #include "config.h"
#ifdef OS2_DRIVER
#define INCL_VIO
#define INCL_KBD
#define INCL_MOU
#include <os2.h>
#include <stdio.h>
#include <sys/signal.h>
#include "aalib.h"
#include "aaint.h"

#define WM_BLACK             0x00
#define WM_BLUE              0x01
#define WM_GREEN             0x02
#define WM_CYAN              0x03
#define WM_RED               0x04
#define WM_MAGENTA           0x05
#define WM_BROWN             0x06
#define WM_PALEGRAY          0x07
#define WM_DKGREY            0x08
#define WM_LBLUE             0x09
#define WM_LGREEN            0x0A
#define WM_LCYAN             0x0B
#define WM_LRED              0x0C
#define WM_LMAGENTA          0x0D
#define WM_YELLOW            0x0E
#define WM_WHITE             0x0F
#define WM_MAX_COLOR         0X0F
#define TOP_ROW          0
#define LEFT_COL         0
#define BOT_ROW          25
#define RGT_COL          80

static VIOCURSORINFO oldCurInfo;
static VIOMODEINFO oldMode, vio_mode;
static BYTE *RowStr;

static int width = 80, height = 32;

void breakfunc(int signo) {
  BYTE bCell[2];
  
  bCell[0] = 0x20;
  bCell[1] = ( WM_BLACK << 4 ) + WM_PALEGRAY;
  
  VioScrollDn(TOP_ROW,LEFT_COL,0xFFFF,0xFFFF,0xFFFF,bCell,(HVIO) 0);
  VioSetMode(&oldMode, (HVIO) 0);
  VioSetCurPos(0, 0, (HVIO)0);
  VioSetCurType(&oldCurInfo, (HVIO)0);

  switch(signo) {
  case SIGINT:
    fprintf(stderr, "Interrupt (Ctrl-C)\n"); break;
  case SIGQUIT:
    fprintf(stderr, "Quit\n"); break;
  case SIGILL:
    fprintf(stderr, "Illegal instruction\n"); break;
  case SIGFPE:
    fprintf(stderr, "Floating point\n"); break;
  case SIGKILL:
    fprintf(stderr, "Kill process\n"); break;
  case SIGBUS:
    fprintf(stderr, "Bus error\n"); break;
  case SIGSEGV:
    fprintf(stderr, "Segmentation fault\n"); break;
  case SIGTERM:
    fprintf(stderr, "Termination, process killed\n"); break;
  case SIGBREAK:
    fprintf(stderr, "Break (Ctrl-Break)\n"); break;
  }
  exit(99);
}

static int os2vio_init(__AA_CONST struct aa_hardware_params *p,__AA_CONST  void *none, struct aa_hardware_params *dest, void *params)
{
  static struct aa_hardware_params def=
  {NULL,
   AA_DIM_MASK | AA_REVERSE_MASK | AA_NORMAL_MASK | AA_BOLD_MASK | AA_EXTENDED,
   0, 0,
   0, 0,
   80, 32,
   0, 0};
  BYTE bCell[2];
  VIOCURSORINFO hidecur;
#ifdef __EMX__
  int idx;
  struct sigaction   sa;
#endif  
  *dest=def;
  
  /* check size or prompt for it */
  if(p->width)
    width = p->width;
  else {
    char c[255];
    width = def.recwidth;
    if(p->recwidth)
      width = p->recwidth;
    printf("Width?[%i]", width);
    gets(c);
    sscanf(c, "%i", &width);
  }
  if (p->height)
    height = p->height;
  else {
    char c[256];
    height = def.recheight;
    if (p->recheight)
      height = p->recheight;
    printf("Height?[%i]", height);
    gets(c);
    sscanf(c, "%i", &height);
  }
  if (p->maxwidth && width > p->maxwidth)
    width = p->maxwidth;
  if (p->minwidth && width < p->minwidth)
    width = p->minwidth;
  if (p->maxheight && height > p->maxheight)
    height = p->maxheight;
  if (p->minheight && height < p->minheight)
    height = p->minheight;

  /* set font for modes that we know the size for, default vga16 */
  switch(height) {
  case 50:
    p->font = &aa_font8; break;
  case 43:
    p->font = &aa_font9; break;
  case 28:
    p->font = &aa_font14; break;
  }

  RowStr = (BYTE *)malloc(2 * width * sizeof(BYTE));
  if(RowStr==NULL) return 0;

  oldMode.cb=sizeof(VIOMODEINFO);
  VioGetMode((PVIOMODEINFO) &oldMode,(HVIO) 0);
  VioGetCurType(&oldCurInfo, (HVIO)0);
  memcpy((void *) &vio_mode, (const void *) &oldMode, sizeof(VIOMODEINFO));
  /* VIO, 80x50 */
  vio_mode.fbType = 1;
  vio_mode.color = 4;
  vio_mode.col = width;
  vio_mode.row = height;
  if(width>=132)
    vio_mode.hres = 1056;
  else
    vio_mode.hres = 720;
  vio_mode.vres = 400;
  vio_mode.fmt_ID = 0;
  vio_mode.attrib = 1;

  if (VioSetMode(&vio_mode, (HVIO) 0)) return 0;

#ifdef __EMX__
  /* under EMX, catch the signals to clean up proberly */

  sa.sa_handler = breakfunc;
  sa.sa_flags   = 0;
  sigemptyset(&sa.sa_mask);
  for (idx=SIGHUP;idx<=SIGTERM;idx++)  sigaction(idx,&sa,NULL);
#endif
  
  hidecur.attr = -1;
  VioSetCurType(&hidecur, (HVIO)0);
  
  bCell[0] = 0x20;
  bCell[1] = ( WM_BLACK << 4 ) + WM_PALEGRAY;
  
  VioScrollDn(TOP_ROW,LEFT_COL,0xFFFF,0xFFFF,0xFFFF,bCell,(HVIO) 0);

#ifdef 0
  printf("minwidth=%d\nminheight=%d\n", p->minwidth, p->minheight);
  printf("maxwidth=%d\nmaxheight=%d\n", p->maxwidth, p->maxheight);
  printf("recwidth=%d\nrecheight=%d\n", p->recwidth, p->recheight);
  printf("mmwidth=%d\nmmheight=%d\n", p->mmwidth, p->mmheight);
  printf("width=%d\nheight=%d\n", p->width, p->height);
  if(p->font!=NULL) {
    printf("fontheight=%d\n", p->font->height);
    printf("fontname=%s\n", p->font->name);
    printf("fontname(short)=%s\n", p->font->shortname);
  }
  getchar();
#endif

  return 1;
}

static void os2vio_uninit(aa_context * c)
{
  BYTE bCell[2];
  
  bCell[0] = 0x20;
  bCell[1] = ( WM_BLACK << 4 ) + WM_PALEGRAY;
  
  VioScrollDn(TOP_ROW,LEFT_COL,0xFFFF,0xFFFF,0xFFFF,bCell,(HVIO) 0);
  VioSetMode(&oldMode, (HVIO) 0);
  VioSetCurPos(0, 0, (HVIO)0);
  VioSetCurType(&oldCurInfo, (HVIO)0);
}

static void os2vio_getsize(aa_context * c, int *w, int *h)
{
  *w = width;
  *h = height;
}

static void os2vio_flush(aa_context * c)
{
  char data[] = {
    (WM_BLACK << 4) + WM_PALEGRAY,    /* NORMAL?   0 */
    (WM_BLACK << 4) + WM_DKGREY,      /* DIM?      1 */
    (WM_BLACK << 4) + WM_WHITE,       /* BOLD?     2 */
    (WM_RED << 4) + WM_PALEGRAY,      /* BOLDFONT? 3 */
    (WM_PALEGRAY << 4) + WM_BLACK,    /* REVERSE?  4 */
    (WM_BLUE << 4) + WM_WHITE         /* SPECIAL?  5 */
  };

  int x, y;
  for (y = 0; y < aa_scrheight(c); y++) {
    for (x = 0; x < aa_scrwidth(c); x++) {
      if(c->attrbuffer[x + y * aa_scrwidth(c)] < 7)
	RowStr[x*2+1] = data[c->attrbuffer[x + y * aa_scrwidth(c)]];
      else
	RowStr[x*2+1] = (WM_BLACK << 4) + WM_PALEGRAY;
      RowStr[x*2] = c->textbuffer[x + y * aa_scrwidth(c)];
    }
    VioWrtCellStr(RowStr, 2*width, y, 0, (HVIO)0);
  }
}
static void os2vio_gotoxy(aa_context * c, int x, int y)
{
  VioSetCurPos(y, x, (HVIO)0);
}

__AA_CONST struct aa_driver os2vio_d =
{
  "os2", "OS/2 VIO driver 0.1",
  os2vio_init,
  os2vio_uninit,
  os2vio_getsize,
  NULL,
  NULL,
  os2vio_gotoxy,
  os2vio_flush,
  NULL
};
#endif
