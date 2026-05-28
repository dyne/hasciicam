// #include "config.h"
#ifdef LINUX_DRIVER
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#ifdef GPM_MOUSEDRIVER
#include <gpm.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include "aalib.h"
#include "aaint.h"
#define MAXVCS 10
static int readonly=1;
static int cursorx, cursory;
static FILE *vc[MAXVCS];
static int nvcs;
static int sizes[2][MAXVCS];
static int cursor_visible = 1;
static void linux_gotoxy (aa_context * c, int x, int y);

static void
linux_cursor (aa_context * c, int mode)
{
  cursor_visible = mode;
  linux_gotoxy (c, cursorx, cursory);
  if (mode)
    {
      printf ("[?25h");
    }
  else
    printf ("[?25l");
  fflush (stdout);
}
static int
linux_init (__AA_CONST struct aa_hardware_params *p, __AA_CONST void *none,
	    struct aa_hardware_params *dest, void **params)
{
  static int registered;
  static struct aa_font font;
  __AA_CONST static struct aa_hardware_params def = { NULL,
    AA_DIM_MASK | AA_REVERSE_MASK | AA_NORMAL_MASK | AA_BOLD_MASK |
      AA_EXTENDED
  };
  struct stat sbuf;
  int major, minor;
  char fname[20];
  char tmp[256], *env;
  int vt;
  int i, y;
  int fd;
  *dest = def;
  fflush (stdout);
  fd = dup (fileno (stderr));
  fstat (fd, &sbuf);
  major = sbuf.st_rdev >> 8;
  vt = minor = sbuf.st_rdev & 0xff;
  close (fd);
  if (major != 4 || minor >= 64)
    return (0);
  readonly = 0;
  if ((env = getenv ("AAVCS")) != NULL)
    {
      int p1 = 0, p2;
      nvcs = 0;
      while (env[p1])
	{
	  while (env[p1] && env[p1] == ' ')
	    p1++;
	  if (!env[p1])
	    break;
	  p2 = 0;
	  while (env[p1] && env[p1] != ' ')
	    tmp[p2++] = env[p1++];
	  tmp[p2] = 0;
	  vc[nvcs] = fopen (tmp, "w+");
	  if (vc[nvcs] == NULL)
	    vc[nvcs] = fopen (tmp, "w"), readonly = 1;
	  if (vc[nvcs] == NULL)
	    return 0;
	  nvcs++;
	}
    }
  else
    {
      sprintf (fname, "/dev/vcsa%i", vt);
      vc[0] = fopen (fname, "w+");
      if (vc[0] == NULL)
	vc[0] = fopen (fname, "w"), readonly = 1;
      nvcs = 1;
    }
  if (vc[0] == NULL)
    return 0;
  if (!registered)
    {
      char *data;
      fd = open ("/dev/console", 0);
      if (fd >= 0)
	{
	  char buf[32 * 1024];
	  struct consolefontdesc desc;
	  desc.chardata = buf;
	  desc.charcount = 1024;
	  i = ioctl (fd, GIO_FONTX, &desc);
	  close (fd);
	  if (i)
	    {			/*probably mda/hercules */
	      dest->font = &aa_font14;
	      dest->supported &= ~AA_DIM_MASK;
	      goto skip;
	    }
	  font.name = "Font used by your console";
	  font.shortname = "current";
	  font.height = desc.charheight;
	  data = malloc (desc.charheight * 256);
	  font.data = data;
	  if (font.data == NULL)
	    goto skip;
	  y = 0;
	  for (i = 0; i < 8192; i++)
	    {
	      if (i % 32 < font.height)
		{
		  data[y] = desc.chardata[i], y++;
		}
	    }
	  aa_registerfont (&font);
	  dest->font = &font;
	}
    }
skip:;
#ifdef GPM_MOUSEDRIVER
  aa_recommendlowmouse ("gpm");
#endif
  aa_recommendlowkbd ("linux");
  aa_recommendlowkbd ("slang");
  aa_recommendlowkbd ("curses");
  return 1;
}
static void
linux_uninit (aa_context * c)
{
  int i;
  for (i = 0; i < nvcs; i++)
    {
      fclose (vc[i]);
    }
}
static void
linux_getsize (aa_context * c, int *width, int *height)
{
  int i;
  struct
  {
    unsigned char lines, cols, x, y;
  }
  scrn =
  {
  0, 0, 0, 0};
  *width = 0;
  *height = 65536;
  if (!readonly)
    {
      for (i = 0; i < nvcs; i++)
	{
	  (void) fseek (vc[i], 0, SEEK_SET);
	  (void) fread (&scrn, 4, 1, vc[i]);
	  sizes[0][i] = scrn.cols;
	  sizes[1][i] = scrn.lines;
	  *width = *width + scrn.cols;
	  if (*height > scrn.lines)
	    *height = scrn.lines;
	}
    }
  else
    {
      struct winsize ws;
      if (ioctl (2, TIOCGWINSZ, &ws) == 0)
	{
	  *width = ws.ws_col * nvcs;
	  *height = ws.ws_row;
	}
      else
	{			/* best guess */
	  *width = 80;
	  *height = 25;
	}
    }
#ifdef GPM_MOUSEDRIVER
  gpm_mx = *width - 1;
  gpm_my = *height - 1;
#endif
}

static void
linux_flush (aa_context * c)
{
  int i;
  int x, y, xstart = 0, xend, end = aa_scrwidth (c) * aa_scrheight (c);
  unsigned char data[] = { 0x07, 0x08, 0x0f, 0x0f, 0x70, 0x17 };
  for (i = 0; i < nvcs; i++)
    {
      fseek (vc[i], 4, 0);
      for (y = 0; y < aa_scrheight (c); y++)
	{
	  int start = y * aa_scrwidth (c);
	  for (x = xstart; x < xstart + sizes[0][i]; x++)
	    {
	      putc (c->textbuffer[x + start], vc[i]);
	      if (c->attrbuffer[x + start] < 7)
		putc (data[c->attrbuffer[x + start]], vc[i]);
	      else
		putc (0x27, vc[i]);
	    }
	}
      xstart += sizes[0][i];
      fflush (vc[i]);
    }
}
static void
linux_gotoxy (aa_context * c, int x, int y)
{
  int n = 0;
  int i;
  struct
  {
    unsigned char lines, cols, x, y;
  }
  scrn;
  cursorx = x;
  cursory = y;
  for (i = 0; i < nvcs; i++)
    {
      (void) fseek (vc[i], 0, SEEK_SET);
      if (x >= n && x < n + sizes[0][i] && cursor_visible)
	{
	  scrn.x = x - n;
	  scrn.y = y;
	  scrn.lines = sizes[0][i];
	  scrn.cols = sizes[1][i];
	}
      else
	{
	  scrn.x = 0;
	  scrn.y = 0;
	  scrn.lines = sizes[0][i];
	  scrn.cols = sizes[1][i];
	}
      (void) fwrite (&scrn, 4, 1, vc[i]);
      n += sizes[0][i];
    }
  fflush (vc[i]);
}
__AA_CONST struct aa_driver linux_d = {
  "linux", "Linux pc console driver 1.0",
  linux_init,
  linux_uninit,
  linux_getsize,
  NULL,
  NULL,
  linux_gotoxy,
  linux_flush,
  linux_cursor
};
#endif
