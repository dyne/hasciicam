/*  HasciiCam 1.3
 *
 *  (c) 2000-2014 Denis Roio <jaromil@dyne.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published
 * by the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Code snippets included by:
 * Josto Chinelli
 * Alessandro Preite Martinez
 * Diego Torres aka Rapid
 * Matteo Scassa aka Blended
 * Hellekin O. Wolf
 * Dan Stowell
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <grp.h>
#endif

#include <aalib.h>
#include "capture/capture_v4l2.h"

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

/* commandline stuff */

char *version =
    "\n%s %s - (h)ascii 4 the masses! - https://ascii.dyne.org\n"
    "(c)2000-2025 RASTASOFT by Jaromil @ Dyne.org\n\n";

char *help =
/* "\x1B" "c" <--- SCREEN CLEANING ESCAPE CODE
   why here? just a reminder for a shamanic secret told by bernie@codewiz.org */
"Usage: hasciicam [options] [rendering options] [aalib options]\n"
"options:\n"
" -h --help         this help\n"
" -H --aahelp       aalib complete help\n"
" -v --version      version information\n"
" -q --quiet        be quiet\n"
" -m --mode         mode: live|html|text      - default live\n"
" -d --device       video grabbing device     - default /dev/video\n"
" -i --input        input channel number      - default 1\n"
" -s --size         ascii image size WxH      - webcam's smallest default\n"
" -o --aafile       dumped file               - default hasciicam.[txt|html]\n"
" -O --aadriver     aalib driver: X11|curses|SDL|stdout - default auto\n"
" -D --daemon       run in background         - default foregrond\n"
" -U --uid          setuid (int)              - default current\n"
" -G --gid          setgid (int)              - default current\n"
"rendering options:\n"
" -S --font-size    html font size (1-4)      - default 1\n"
" -a --font-face    html font to use          - default courier\n"
" -r --refresh      refresh delay             - default 2\n"
" -b --aabright     ascii brightness          - default 60\n"
" -c --aacontrast   ascii contrast            - default 4\n"
" -g --aagamma      ascii gamma               - default 3\n"
" -I --invert       invert colors             - default off\n"
" -B --background   background color (hex)    - default 000000\n"
" -F --foreground   foreground color (hex)    - default 00FF00\n";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"aahelp", no_argument, NULL, 'H'},
  {"version", no_argument, NULL, 'v'},
  {"quiet", no_argument, NULL, 'q'},
  {"mode", required_argument, NULL, 'm'},
  {"device", required_argument, NULL, 'd'},
  {"input", required_argument, NULL, 'i'},
  {"size", required_argument, NULL, 's'},
  {"aafile", required_argument, NULL, 'o'},
  {"aadriver", required_argument, NULL, 'O'},
  {"daemon", no_argument, NULL, 'D'},
  {"font-size", required_argument, NULL, 'S'},
  {"font-face", required_argument, NULL, 'a'},
  {"refresh", required_argument, NULL, 'r'},
  {"aabright", required_argument, NULL, 'b'},
  {"aacontrast", required_argument, NULL, 'c'},
  {"aagamma", required_argument, NULL, 'g'},
  {"invert", no_argument, NULL, 'I'},
  {"background", required_argument, NULL, 'B'},
  {"foreground", required_argument, NULL, 'F'},
  {"uid", required_argument, NULL, 'U'},
  {"gid", required_argument, NULL, 'G'},
  {0, 0, 0, 0}
};

char *short_options = "hHvqm:d:i:s:f:DS:a:r:o:b:c:g:IB:F:O:Q:U:G:";

/* default configuration */
int quiet = 0;
int mode = 0;
int inputch = 0;
int daemon_mode = 0;
int invert = 0;

struct geometry aa_geo;
struct geometry vid_geo;
/* if width&height have been manually changed */
int whchanged = 0;

char device[256];
int have_tuner = 0;

int refresh = 2;
int fontsize = 1;
int linespace = 5;
char background[64];
char foreground[64];
char fontface[256];
char aadriver[64];

int user_w = 0;
int user_h = 0;

int uid = -1;
int gid = -1;

/* buffers */
unsigned char *image = NULL; /* mmapped */
char aafile[256];
char aatmpfile[256];


/* declare the sighandler */
void quitproc (int Sig);
volatile sig_atomic_t userbreak;

/* ascii context & html formatting stuff*/
aa_context *ascii_context;
struct aa_renderparams *ascii_rndparms;
struct aa_hardware_params ascii_hwparms;
struct aa_savedata ascii_save;

char hascii_header[1024];

const char * const html_escapes[] =
  { "<", "&lt;", ">", "&gt;", "&", "&amp;", NULL };

struct aa_format hascii_format = {
  79, 25,
  0, 0,
  0,
  AA_NORMAL_MASK | AA_BOLD_MASK | AA_BOLDFONT_MASK,
  NULL,
  "Pure html",
  ".html",
  hascii_header,
  "</PRE>\n</FONT>\n</BODY>\n</HTML>\n",
  "\n",
  /*The order is:normal, dim, bold, boldfont, reverse, special */
  {"%s", "%s", "%s", "%s", "%s"},
  {"", "", "<B>", "", "<B>"},
  {"", "", "</B>", "", "</B>"},
  html_escapes
};

/* greyscale image is sampled from Y luminance component */
unsigned char *grey;
int YtoRGB[256];
int xstep=2, ystep=4;
int xbytestep;
int ybytestep;
int renderhop=2, framenum=0; // renderhop is how many frames to guzzle before rendering
int gw, gh; // number of cols/rows in grey intermediate representation
int vw, vh; // video w and h
int aw, ah; // ascii w and h
size_t greysize;
int vbytesperline;

void YUV422_to_grey(unsigned char *src, unsigned char *dst, int w, int h) {
    unsigned char *writehead, *readhead;
    int x,y;
    writehead = dst;
    readhead  = src;
    for(y=0; y<gh; ++y){
        for(x=0; x<gw; ++x){
            *(writehead++) = *readhead;
            readhead += xbytestep;
        }
        readhead += ybytestep;
    }
}

/* New function to scale YUV422 to grey at target dimensions */
void YUV422_to_grey_scaled(unsigned char *src, unsigned char *dst, int src_w, int src_h, int dst_w, int dst_h) {
    unsigned char *writehead = dst;
    unsigned char *readhead = src;

    /* Calculate scaling factors */
    float x_scale = (float)src_w / (float)dst_w;
    float y_scale = (float)src_h / (float)dst_h;

    int xstep_bytes = (int)(x_scale * 2); /* YUV422 has 2 bytes per pixel */
    int ystep_lines = (int)(y_scale);

    if (xstep_bytes < 2) xstep_bytes = 2;
    if (ystep_lines < 1) ystep_lines = 1;

    int y_stride = src_w * 2; /* YUV422: 2 bytes per pixel */

    for(int y = 0; y < dst_h; ++y) {
        /* Calculate source row */
        int src_y = (int)(y * y_scale);
        unsigned char *row_ptr = src + src_y * y_stride;

        for(int x = 0; x < dst_w; ++x) {
            /* Calculate source column */
            int src_x = (int)(x * x_scale);
            /* In YUV422, we want the Y component, which is at even byte positions */
            int src_byte = src_x * 2;
            *(writehead++) = row_ptr[src_byte];
        }
    }
}

void
config_init (int argc, char *argv[]) {
  int res;

  /* setup defaults */

  { /* device filename */
    struct stat st;
    if( stat("/dev/video",&st) <0)
      strcpy(device,"/dev/video0");
    else
      strcpy(device,"/dev/video");
  }
  strcpy(background,"000000");
  strcpy(foreground,"00FF00");
  strcpy(fontface,"courier"); /* you'd better choose monospace fonts */
  strcpy(aadriver,""); /* empty means auto-detect */

  aa_geo.w = 80; // 96;
  aa_geo.h = 40; // 72;
  aa_geo.bright =  60;
  aa_geo.contrast = 4;
  aa_geo.gamma = 3;

  do {
    res = getopt_long (argc, argv, short_options, long_options, NULL);

    switch (res) {
    case 'h':
      fprintf (stderr, "%s", help);
      exit (0);
      break;
    case 'H':
      fprintf (stderr, "%s", help);
      fprintf (stderr, "\naalib options:\n%s",aa_help);
      exit(0);
    case 'v':
      exit (0);
      break;
    case 'q':
      quiet = 1;
      break;
    case 'm':
      if (strcasecmp (optarg, "live") == 0) {
        mode = LIVE;
      } else if (strcasecmp (optarg, "html") == 0) {
        mode = HTML;
        strcpy(aafile,"hasciicam.html");
      } else if (strcasecmp (optarg, "text") == 0) {
        mode = TEXT;
        strcpy(aafile,"hasciicam.asc");
      } else {
        fprintf (stderr, "!! invalid mode selected, using live\n");
        mode = LIVE;
      }
      break;
    case 'd':
      strncpy(device,optarg,256);
      break;
    case 'i':
      inputch = atoi (optarg);
      /*
	 here we assume that capture cards have maximum 3 channels
	 (usually the 4th, when present, is the radio tuner)
      */
      if (inputch > 3) {
	fprintf (stderr, "invalid input selected\n");
	exit (1);
      }
      break;

    case 's':
      {
	char *t;
	char *tt;
	t = optarg;
	while (isdigit (*t))
	  t++;
	*t = 0;
	user_w = atoi (optarg);
	tt = ++t;
	while (isdigit (*tt))
	  tt++;
	*tt = 0;
	user_h = atoi (t);
	whchanged = 1;
      }
      break;
    case 'S':
      fontsize = atoi (optarg);
      switch (fontsize) {
      case 1: linespace = 5; break;
      case 2: linespace = 10; break;
      case 3: linespace = 11; break;
      case 4: linespace = 13; break;
      default: linespace = 15; break;
      }
      break;
    case 'a':
      strncpy(fontface,optarg,256);
      break;
    case 'r':
      refresh = atoi (optarg);
      break;
    case 'o':
      if(mode>0)
	strncpy(aafile,optarg,256);
      break;
    case 'O':
      strncpy(aadriver,optarg,64);
      break;
    case 'D':
      daemon_mode = 1;
      break;
    case 'b':
      aa_geo.bright = atoi (optarg);
      break;
    case 'c':
      aa_geo.contrast = atoi (optarg);
      break;
    case 'g':
      aa_geo.gamma = atoi (optarg);
      break;
    case 'I':
      invert = 1;
      break;
    case 'B':
      strncpy(background,optarg,64);
      break;
    case 'F':
      strncpy(foreground,optarg,64);
      break;
    case 'U':
      uid = atoi (optarg);
      break;
    case 'G':
      gid = atoi (optarg);
      break;
    }
  } while (res > 0);

}

/* here we go (chmicl broz rlz! :)*/

int
main (int argc, char **argv) {

    /* reminder:
       !!! grabbing height & width should be double
       the ascii context width and height !!! */

  /* register signal traps */
  if (signal (SIGINT, quitproc) == SIG_ERR) {
      perror ("Couldn't install SIGINT handler"); exit (1); }
  fprintf (stderr, version, PACKAGE, VERSION);

  /* default values */
#if defined(_WIN32)
  uid = 0;
  gid = 0;
#else
  uid = getuid ();
  gid = getgid ();
#endif


  /* initialize aalib default params */
  memcpy (&ascii_hwparms, &aa_defparams, sizeof (struct aa_hardware_params));
  ascii_rndparms = aa_getrenderparams();

  /* gathering aalib commandline options */
  aa_parseoptions (&ascii_hwparms, ascii_rndparms, &argc, argv);

  /* set hasciicam options */
  config_init (argc, argv);
  /* detect and init video device */
  if( vid_detect(device) > 0 ) {
    vid_init();
  } else
    exit(-1);

  /* width/height image setup */
  ascii_hwparms.font = NULL; // default font, thanks
  /* Use recommended dimensions instead of exact for flexibility with DPI scaling */
  ascii_hwparms.width = 0;
  ascii_hwparms.height = 0;
  ascii_hwparms.recwidth = aw;
  ascii_hwparms.recheight = ah;


  /* init the html header */
  snprintf (&hascii_header[0], 1024,
	    "<HTML>\n <HEAD> <TITLE>wow! (h)ascii 4 the masses!</TITLE>\n"
	    "<META HTTP-EQUIV=\"refresh\" CONTENT=\"%u\"; url=\"%s\">\n"
	    "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">\n"
	    "<STYLE TYPE=\"text/css\">\n"
	    "<!--\npre {\nletter-spacing: 1px;\n"
	    "layer-background-color: Black;\n"
	    "left : auto;\nline-height : %upx;\n}\n-->\n"
	    "</STYLE>\n</HEAD>\n<BODY bgcolor=\"#%s\" text=\"#%s\">\n"
	    "<FONT SIZE=%u face=\"%s\">\n<PRE>\n",
	    refresh, aafile, linespace, background, foreground, fontsize,
	    fontface);

#if !defined(_WIN32)
  setgroups(0, NULL);
  setuid (uid);
  setgid (gid);
#endif

  fprintf (stderr, "Ascii size is %dx%d\n", aw, ah);

  switch (mode)
    {
    case LIVE:
      fprintf (stderr, "using LIVE mode\n");
      break;

    case HTML:
      snprintf(aatmpfile,255,"%s.tmp",aafile);
      ascii_save.name = aatmpfile;
      ascii_save.format = &hascii_format;
      ascii_save.file = NULL;

      fprintf (stderr, "using HTML mode dumping to file %s\n", aafile);
      break;

    case TEXT:
      ascii_save.name = aafile;
      ascii_save.format = &aa_text_format;
      ascii_save.file = NULL;

      fprintf (stderr, "using TEXT mode dumping to file %s\n", aafile);

      break;

    default:
      break;
    }

  fprintf(stderr,"\n");

  /* aalib init */
  if (mode > 0) {
    fprintf(stderr,"Using save mode with file output\n");
    ascii_context = aa_init (&save_d, &ascii_hwparms, &ascii_save);
  } else {
    /* set driver preferences */
    if(strlen(aadriver) > 0) {
      /* user specified a driver */
      if(!quiet)
        fprintf(stderr,"Driver preference: %s\n", aadriver);
      aa_recommendhidisplay(aadriver);
    } else {
      /* default: prefer SDL, then X11, then text-based */
      if(!quiet)
        fprintf(stderr,"Auto-detecting display driver (SDL preferred)\n");
      aa_recommendhidisplay("SDL");
      aa_recommendhidisplay("X11");
      aa_recommendhidisplay("curses");
      aa_recommendhidisplay("linux");
      aa_recommendhidisplay("stdout");
    }
    ascii_context = aa_autoinit (&ascii_hwparms);
  }

  if(!ascii_context) {
    fprintf(stderr,"!! cannot initialize aalib\n");
    if(strlen(aadriver) > 0) {
      fprintf(stderr,"!! failed to initialize '%s' driver\n", aadriver);
      fprintf(stderr,"!! try: -O curses for terminal mode\n");
    }
    exit(-1);
  }

  /* report which driver was actually initialized */
  if(!quiet && ascii_context->driver) {
    fprintf(stderr,"Using driver: %s (%s)\n",
            ascii_context->driver->shortname,
            ascii_context->driver->name);
  }


  ascii_rndparms->bright = aa_geo.bright;
  ascii_rndparms->contrast = aa_geo.contrast;
  ascii_rndparms->gamma = (float)aa_geo.gamma;
  // those are left to be setted by aalib options
  //  ascii_rndparms->dither = AA_FLOYD_S;
  //  ascii_rndparms->inversion = invert;
  //  ascii_rndparms->randomval = 0;




//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


  if (daemon_mode)
#if defined(_WIN32)
    fprintf(stderr, "daemon mode is not supported on this platform\n");
#else
    daemon (0, 1);
#endif




  while (userbreak <1) {
    grab_one ();
	/*aa_setpalette (gamma di colori, indice, colore rosso, verde, blu)*/

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//    memcpy (aa_image (ascii_context), grey, vid_geo.size);
//    aa_render (ascii_context, ascii_rndparms, 0, 0,
//	       vid_geo.w,vid_geo.h);

    aa_flush (ascii_context);
  //  unlink(aafile);
    rename(aatmpfile,aafile);

  }

  /* CLEAN EXIT */

  vid_close();
  aa_close(ascii_context);
  free(grey);
  fprintf (stderr, "cya!\n");
  exit (0);
/*++userbreak;*/
}

/* signal handling */
void
quitproc (int Sig)
{

  fprintf (stderr, "interrupt caught, exiting.\n");
  userbreak = 1;
}
