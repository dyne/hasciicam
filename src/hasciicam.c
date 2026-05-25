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
#include "app/app_config.h"
#include "app/app_session.h"
#include "capture/capture_backend.h"
#include "output/output.h"
#include "output/output_file.h"
#include "render/render_session.h"

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

/* default configuration */
static hasciicam_config appcfg;
extern int quiet;

struct geometry {
  int w, h, size;
  int bright, contrast, gamma;
};

int have_tuner = 0;

/* buffers */
unsigned char *image = NULL; /* mmapped */
char aatmpfile[256];


/* declare the sighandler */
void quitproc (int Sig);
volatile sig_atomic_t userbreak;

/* greyscale image is sampled from Y luminance component */
int YtoRGB[256];
int xstep=2, ystep=4;
int xbytestep;
int ybytestep;
int renderhop=2, framenum=0; // renderhop is how many frames to guzzle before rendering
int gw, gh; // number of cols/rows in grey intermediate representation
int vw, vh; // video w and h
int aw, ah; // ascii w and h
int vbytesperline;

/* here we go (chmicl broz rlz! :)*/

int
main (int argc, char **argv) {
  capture_request cap_req;
  const capture_info *cap_info = NULL;
  hasciicam_session session;
  hasciicam_render_session render_session;
  hasciicam_output output;
  struct geometry aa_geo;
  struct geometry vid_geo;
  int mode;
  int inputch;
  int daemon_mode;
  int refresh;
  int fontsize;
  int linespace;
  int user_w;
  int user_h;
  int whchanged;
  int uid;
  int gid;
  char *device;
  char *aafile;
  char *background;
  char *foreground;
  char *fontface;
  char *aadriver;

    /* reminder:
       !!! grabbing height & width should be double
       the ascii context width and height !!! */

  /* register signal traps */
  if (signal (SIGINT, quitproc) == SIG_ERR) {
      perror ("Couldn't install SIGINT handler"); exit (1); }
  fprintf(stderr,
          "\n%s %s - (h)ascii 4 the masses! - https://ascii.dyne.org\n"
          "(c)2000-2025 RASTASOFT by Jaromil @ Dyne.org\n\n",
          PACKAGE, VERSION);

  hasciicam_config_init_defaults(&appcfg);

  /* default values */
#if defined(_WIN32)
  uid = 0;
  gid = 0;
#else
  uid = getuid ();
  gid = getgid ();
#endif


  /* initialize render session defaults */
  hasciicam_render_session_init(&render_session);

  /* gathering aalib commandline options */
  aa_parseoptions(&render_session.hwparams, render_session.render_params, &argc, argv);

  /* set hasciicam options */
  hasciicam_config_parse(&appcfg, argc, argv, aa_help, PACKAGE, VERSION);
  quiet = appcfg.quiet;
  mode = appcfg.mode;
  inputch = appcfg.input_channel;
  daemon_mode = appcfg.daemon_mode;
  refresh = appcfg.refresh;
  fontsize = appcfg.fontsize;
  linespace = appcfg.linespace;
  user_w = appcfg.user_w;
  user_h = appcfg.user_h;
  whchanged = appcfg.whchanged;
  uid = appcfg.uid;
  gid = appcfg.gid;
  device = appcfg.device;
  aafile = appcfg.aafile;
  background = appcfg.background;
  foreground = appcfg.foreground;
  fontface = appcfg.fontface;
  aadriver = appcfg.aadriver;
  aa_geo.w = 80;
  aa_geo.h = 40;
  aa_geo.bright = appcfg.aa_bright;
  aa_geo.contrast = appcfg.aa_contrast;
  aa_geo.gamma = appcfg.aa_gamma;
  memset(&cap_req, 0, sizeof(cap_req));
  cap_req.device = device;
  cap_req.input_channel = inputch;
  if (whchanged == 1) {
    cap_req.requested_width = user_w;
    cap_req.requested_height = user_h;
  }

  if (!hasciicam_session_start(&session, &cap_req)) {
    fprintf(stderr, "!! cannot open any capture backend: %s\n",
            capture_last_error());
    exit(-1);
  }
  cap_info = hasciicam_session_capture_info(&session);
  if (cap_info == NULL) {
    fprintf(stderr, "!! cannot query capture backend\n");
    hasciicam_session_stop(&session);
    exit(-1);
  }

  vw = cap_info->width;
  vh = cap_info->height;
  vbytesperline = cap_info->stride_bytes;
  vid_geo.w = vw;
  vid_geo.h = vh;
  vid_geo.size = vw * vh;

  xbytestep = xstep + xstep;
  ybytestep = vbytesperline * (ystep - 1);
  gw = vw / xstep;
  gh = vh / ystep;
  aw = gw / 2;
  ah = gh / 2;

  fprintf(stderr, "Grey buffer is %i bytes\n", gw * gh);

  hasciicam_render_session_configure_geometry(&render_session, aw, ah);
  hasciicam_render_session_prepare_html(&render_session, refresh, aafile, linespace,
                                        background, foreground, fontsize, fontface);

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
      hasciicam_output_file_prepare(&render_session, mode, aafile,
                                    aatmpfile, sizeof(aatmpfile));

      fprintf (stderr, "using HTML mode dumping to file %s\n", aafile);
      break;

    case TEXT:
      hasciicam_output_file_prepare(&render_session, mode, aafile,
                                    aatmpfile, sizeof(aatmpfile));

      fprintf (stderr, "using TEXT mode dumping to file %s\n", aafile);

      break;

    default:
      break;
    }

  fprintf(stderr,"\n");

  if (mode > 0) {
    fprintf(stderr,"Using save mode with file output\n");
  }
  if (!hasciicam_render_session_open(&render_session, mode, aadriver, quiet)) {
    fprintf(stderr,"!! cannot initialize aalib\n");
    if(strlen(aadriver) > 0) {
      fprintf(stderr,"!! failed to initialize '%s' driver\n", aadriver);
      fprintf(stderr,"!! try: -O curses for terminal mode\n");
    }
    exit(-1);
  }

  /* report which driver was actually initialized */
  if(!quiet && render_session.context->driver) {
    fprintf(stderr,"Using driver: %s (%s)\n",
            render_session.context->driver->shortname,
            render_session.context->driver->name);
  }
  if (!hasciicam_output_open_aalib(&output, render_session.context)) {
    fprintf(stderr, "!! cannot initialize output adapter\n");
    hasciicam_session_stop(&session);
    hasciicam_render_session_close(&render_session);
    exit(-1);
  }

  hasciicam_render_session_apply_tuning(&render_session,
                                        aa_geo.bright,
                                        aa_geo.contrast,
                                        (float)aa_geo.gamma);
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




  while (userbreak <1 && !hasciicam_session_should_stop(&session)) {
    const unsigned char *gray_frame = NULL;
    int gray_size = 0;
    int frame_rendered = 0;

    if (!hasciicam_session_step(&session, aa_imgwidth(render_session.context),
                                aa_imgheight(render_session.context),
                                &gray_frame, &gray_size)) {
      hasciicam_session_request_stop(&session);
      break;
    }

    if ((++framenum) == renderhop) {
      int ascii_width = aa_imgwidth(render_session.context);
      int ascii_height = aa_imgheight(render_session.context);
      int dest_size;
      int copy_size;

      framenum = 0;
      dest_size = aa_imgwidth(render_session.context) * aa_imgheight(render_session.context);
      copy_size = (gray_size < dest_size) ? gray_size : dest_size;
      if (copy_size > 0) {
        memcpy(aa_image(render_session.context), gray_frame, copy_size);
        hasciicam_output_write_ascii_frame(&output, ascii_width, ascii_height);
        frame_rendered = 1;
      }
    }
	/*aa_setpalette (gamma di colori, indice, colore rosso, verde, blu)*/

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//    aa_render (ascii_context, ascii_rndparms, 0, 0,
//	       vid_geo.w,vid_geo.h);

    if (mode == LIVE) {
      hasciicam_output_poll(&output);
    } else if (mode == HTML && frame_rendered) {
      rename(aatmpfile, aafile);
    }

  }

  /* CLEAN EXIT */

  hasciicam_session_stop(&session);
  hasciicam_output_close(&output);
  hasciicam_render_session_close(&render_session);
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
