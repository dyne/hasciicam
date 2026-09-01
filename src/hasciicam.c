/*  HasciiCam
 *
 *  (c) 2000-2026 Dyne.org foundation
 *  designed, written and maintained by Denis Roio <jaromil@dyne.org>
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
#include "app/app_live_controls.h"
#include "app/app_virtual_camera.h"
#include "app/app_size.h"
#include "app/app_session.h"
#include "capture/capture_backend.h"
#include "display/display_size.h"
#include "gui/gui_bridge.h"
#include "gui/gui_file_dialog.h"
#include "gui/gui_state.h"
#include "output/output.h"
#include "output/output_file.h"
#include "output/output_text_frame.h"
#include "render/render_session.h"
#include "render/render_font.h"

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

static void set_process_env(const char *name, const char *value) {
  if (name == NULL || value == NULL)
    return;
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

static void apply_sdl_runtime_options(const hasciicam_config *cfg) {
  if (cfg == NULL)
    return;
  if (cfg->sdl_renderer[0] != '\0')
    set_process_env("HASCIICAM_SDL_RENDERER", cfg->sdl_renderer);
  if (cfg->sdl_vsync == 0)
    set_process_env("HASCIICAM_SDL_VSYNC", "off");
  else if (cfg->sdl_vsync == 1)
    set_process_env("HASCIICAM_SDL_VSYNC", "on");
  else if (cfg->sdl_vsync == -1)
    set_process_env("HASCIICAM_SDL_VSYNC", "auto");
  if (cfg->sdl_fullscreen)
    set_process_env("HASCIICAM_SDL_FULLSCREEN", "1");
}

static void sync_virtual_camera_gui_state(hasciicam_gui_state *gui_state,
                                          const hasciicam_app_virtual_camera *virtual_camera) {
  const hasciicam_virtual_camera_request *request;
  const char *backend_name = "";

  if (gui_state == NULL || virtual_camera == NULL)
    return;
  request = hasciicam_app_virtual_camera_request(virtual_camera);
  backend_name = hasciicam_app_virtual_camera_backend_name(virtual_camera);
  hasciicam_gui_state_set_virtual_camera(gui_state,
                                         request != NULL ? request->enabled : 0,
                                         backend_name,
                                         "HasciiCam",
                                         request != NULL ? request->device : "",
                                         request != NULL ? request->width : 0,
                                         request != NULL ? request->height : 0,
                                         request != NULL ? request->fps : 0,
                                         virtual_camera->active,
                                         hasciicam_app_virtual_camera_accepted_frames(virtual_camera),
                                         hasciicam_app_virtual_camera_dropped_frames(virtual_camera));
}

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
  capture_request camera_req;
  const capture_info *cap_info = NULL;
  hasciicam_session session;
  hasciicam_render_session render_session;
  hasciicam_gui_state gui_state;
  hasciicam_output output;
  hasciicam_app_virtual_camera virtual_camera;
  capture_control_desc control_descs[CAPTURE_MAX_CONTROLS];
  int control_count = 0;
  struct geometry aa_geo;
  struct geometry vid_geo;
  int mode;
  int inputch;
  int daemon_mode;
  int refresh;
  int fontsize;
  int linespace;
  hasciicam_size_metrics size_metrics;
  hasciicam_size_plan size_plan;
  int uid;
  int gid;
  int max_frames;
  int rendered_frames = 0;
  int detected_screen_w = 0;
  int detected_screen_h = 0;
  int auto_live_size_applied = 0;
  int is_image_source = 0;
  hasciicam_font_desc selected_font;
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
          "(c)2000-2026 RASTASOFT by Jaromil @ Dyne.org\n\n",
          PACKAGE, VERSION);

  hasciicam_config_init_defaults(&appcfg);
  hasciicam_app_virtual_camera_init(&virtual_camera);

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
  apply_sdl_runtime_options(&appcfg);
  quiet = appcfg.quiet;
  mode = appcfg.mode;
  inputch = appcfg.input_channel;
  daemon_mode = appcfg.daemon_mode;
  refresh = appcfg.refresh;
  fontsize = appcfg.fontsize;
  linespace = appcfg.linespace;
  uid = appcfg.uid;
  gid = appcfg.gid;
  max_frames = appcfg.max_frames;
  device = appcfg.device;
  aafile = appcfg.aafile;
  background = appcfg.background;
  foreground = appcfg.foreground;
  fontface = appcfg.fontface;
  aadriver = appcfg.aadriver;

  if (appcfg.virtual_camera && mode != LIVE) {
    fprintf(stderr, "!! virtual camera is only available in live mode\n");
    exit(-1);
  }
  selected_font = hasciicam_font_find(appcfg.font);
  if (selected_font.font == 0) {
    fprintf(stderr, "!! unknown font '%s'\n", appcfg.font);
    exit(1);
  }
  render_session.hwparams.font = selected_font.font;
  aa_geo.w = 80;
  aa_geo.h = 40;
  aa_geo.bright = appcfg.aa_bright;
  aa_geo.contrast = appcfg.aa_contrast;
  aa_geo.gamma = appcfg.aa_gamma;
  hasciicam_size_metrics_init(&size_metrics);
  size_metrics.display_pixels_per_char_y = selected_font.height;
  hasciicam_size_build_plan(&appcfg, &size_metrics, &size_plan);

  if (mode == LIVE && !appcfg.explicit_size && !appcfg.explicit_aadriver) {
    if (!appcfg.explicit_aadriver && appcfg.aadriver[0] == '\0') {
      strncpy(appcfg.aadriver, "SDL", sizeof(appcfg.aadriver) - 1);
      appcfg.aadriver[sizeof(appcfg.aadriver) - 1] = '\0';
      aadriver = appcfg.aadriver;
    }
    if (hasciicam_display_size_detect_primary(&detected_screen_w, &detected_screen_h)) {
      hasciicam_size_build_default_live_plan(&size_metrics,
                                             detected_screen_w,
                                             detected_screen_h,
                                             &size_plan);
      auto_live_size_applied = 1;
    }
  }

  memset(&camera_req, 0, sizeof(camera_req));
  camera_req.device = device;
  camera_req.input_channel = inputch;
  if (size_plan.requested_capture_width > 0 && size_plan.requested_capture_height > 0) {
    camera_req.requested_width = size_plan.requested_capture_width;
    camera_req.requested_height = size_plan.requested_capture_height;
  }
  cap_req = camera_req;
  cap_req.image_path = appcfg.image;

  if (!hasciicam_session_start(&session, &cap_req)) {
    fprintf(stderr, "!! cannot open any capture backend: %s\n",
            capture_last_error());
    exit(-1);
  }
  hasciicam_session_set_mirror(&session, appcfg.mirror_x, appcfg.mirror_y);
  cap_info = hasciicam_session_capture_info(&session);
  if (cap_info == NULL) {
    fprintf(stderr, "!! cannot query capture backend\n");
    hasciicam_session_stop(&session);
    exit(-1);
  }

  vw = cap_info->width;
  vh = cap_info->height;
  is_image_source = session.capture_ops != NULL &&
                    strcmp(session.capture_ops->name(), "image") == 0;
  vbytesperline = cap_info->stride_bytes;
  vid_geo.w = vw;
  vid_geo.h = vh;
  vid_geo.size = vw * vh;

  xbytestep = xstep + xstep;
  ybytestep = vbytesperline * (ystep - 1);
  hasciicam_size_resolve_ascii(&size_metrics, &size_plan,
                               appcfg.explicit_size && is_image_source,
                               vw, vh, &aw, &ah);
  if (mode == LIVE && is_image_source &&
      hasciicam_display_size_detect_primary(&detected_screen_w, &detected_screen_h)) {
    hasciicam_size_fit_ascii_to_display(&size_metrics,
                                        detected_screen_w, detected_screen_h,
                                        &aw, &ah);
  }
  gw = aw * 2;
  gh = ah * 2;

  if (!quiet && appcfg.size_intent != HASCIICAM_SIZE_NONE) {
    const char *intent = (appcfg.size_intent == HASCIICAM_SIZE_PIXELS) ? "pixels" : "chars";
    fprintf(stderr, "Size request: %s %dx%d\n", intent, appcfg.size_w, appcfg.size_h);
    if (cap_req.requested_width > 0 && cap_req.requested_height > 0) {
      fprintf(stderr, "Capture target: %dx%d\n",
              cap_req.requested_width, cap_req.requested_height);
    }
    if (appcfg.size_intent == HASCIICAM_SIZE_PIXELS) {
      fprintf(stderr, "Window pixel target: %dx%d\n", appcfg.size_w, appcfg.size_h);
      fprintf(stderr, "Ascii grid target: %dx%d\n",
              size_plan.preferred_ascii_width, size_plan.preferred_ascii_height);
    }
    fprintf(stderr, "Capture negotiated: %dx%d\n", vw, vh);
    fprintf(stderr, "Ascii result: %dx%d\n", aw, ah);
  } else if (!quiet && auto_live_size_applied) {
    fprintf(stderr, "Auto live sizing from primary display: %dx%d\n",
            detected_screen_w, detected_screen_h);
    fprintf(stderr, "Capture target: %dx%d\n",
            size_plan.requested_capture_width, size_plan.requested_capture_height);
    fprintf(stderr, "Capture negotiated: %dx%d\n", vw, vh);
    fprintf(stderr, "Ascii result: %dx%d\n", aw, ah);
  }

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

  if (mode == LIVE && appcfg.virtual_camera) {
    char virtual_camera_err[256];
    if (render_session.context == NULL || render_session.context->driver == NULL ||
        strcmp(render_session.context->driver->shortname, "SDL") != 0) {
      fprintf(stderr, "!! virtual camera requires the SDL live driver\n");
      hasciicam_session_stop(&session);
      hasciicam_render_session_close(&render_session);
      exit(-1);
    }
    virtual_camera_err[0] = '\0';
    if (!hasciicam_app_virtual_camera_start(&virtual_camera,
                                            render_session.context,
                                            &appcfg,
                                            hasciicam_sdl_set_frame_callback,
                                            virtual_camera_err,
                                            sizeof(virtual_camera_err))) {
      fprintf(stderr, "!! cannot initialize virtual camera: %s\n",
              virtual_camera_err[0] ? virtual_camera_err : "unknown error");
      hasciicam_session_stop(&session);
      hasciicam_render_session_close(&render_session);
      exit(-1);
    }
    sync_virtual_camera_gui_state(&gui_state, &virtual_camera);
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
  render_session.render_params->inversion = appcfg.invert ? 1 : 0;
  hasciicam_gui_state_init(&gui_state, &appcfg);
  hasciicam_gui_state_set_capture_info(&gui_state, cap_info);
  control_count = hasciicam_session_list_controls(&session, control_descs, CAPTURE_MAX_CONTROLS);
  hasciicam_gui_state_set_capture_controls(&gui_state, control_descs, control_count);
  sync_virtual_camera_gui_state(&gui_state, &virtual_camera);
  hasciicam_sdl_set_runtime_colors(render_session.context,
                                   gui_state.foreground_rgb,
                                   gui_state.background_rgb,
                                   gui_state.aa_dimmer);
  hasciicam_sdl_set_gui_state(render_session.context, &gui_state);
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
    char cfg_err[256];

    if (gui_state.open_load_dialog_requested) {
      hasciicam_gui_file_dialog_result dialog_res;
      char dialog_err[256];
      dialog_err[0] = '\0';
      gui_state.open_load_dialog_requested = 0;
      dialog_res = hasciicam_gui_select_toml_file(gui_state.load_path,
                                                  sizeof(gui_state.load_path),
                                                  dialog_err,
                                                  sizeof(dialog_err));
      if (dialog_res == HASCIICAM_GUI_FILE_DIALOG_SELECTED) {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "selected: %s", gui_state.load_path);
        gui_state.status_is_error = 0;
      } else if (dialog_res == HASCIICAM_GUI_FILE_DIALOG_ERROR) {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "%s", dialog_err[0] ? dialog_err : "file dialog error");
        gui_state.status_is_error = 1;
      }
    }

    if (gui_state.open_image_dialog_requested) {
      hasciicam_gui_file_dialog_result dialog_res;
      char dialog_err[256];
      dialog_err[0] = '\0';
      gui_state.open_image_dialog_requested = 0;
      dialog_res = hasciicam_gui_select_file(HASCIICAM_GUI_FILE_DIALOG_IMAGE,
                                              gui_state.image_path,
                                              sizeof(gui_state.image_path),
                                              dialog_err,
                                              sizeof(dialog_err));
      if (dialog_res == HASCIICAM_GUI_FILE_DIALOG_SELECTED) {
        hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                       gui_state.source_label,
                                       "image selected", 0);
      } else if (dialog_res == HASCIICAM_GUI_FILE_DIALOG_NOT_AVAILABLE) {
        hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                       gui_state.source_label,
                                       "Browse is unavailable; enter an image path.", 0);
      } else if (dialog_res == HASCIICAM_GUI_FILE_DIALOG_ERROR) {
        hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                       gui_state.source_label,
                                       dialog_err[0] ? dialog_err : "file dialog error", 1);
      }
    }

    if (gui_state.load_image_requested || gui_state.use_camera_requested) {
      capture_request requested = camera_req;
      int loading_image = gui_state.load_image_requested != 0;
      gui_state.load_image_requested = 0;
      gui_state.use_camera_requested = 0;
      if (loading_image)
        requested.image_path = gui_state.image_path;
      if (loading_image && requested.image_path[0] == '\0') {
        hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                       gui_state.source_label, "enter an image path", 1);
      } else if (!hasciicam_session_replace(&session, &requested)) {
        hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                       gui_state.source_label, capture_last_error(), 1);
      } else {
        int replacement_aw;
        int replacement_ah;
        int display_resize_ok = 1;
        cap_info = hasciicam_session_capture_info(&session);
        hasciicam_size_resolve_ascii(&size_metrics, &size_plan, appcfg.explicit_size,
                                     cap_info->width, cap_info->height,
                                     &replacement_aw, &replacement_ah);
        if (mode == LIVE && !appcfg.explicit_size &&
            !hasciicam_sdl_set_grid_size(render_session.context,
                                         replacement_aw, replacement_ah)) {
          display_resize_ok = 0;
        }
        hasciicam_gui_state_reset_preview(&gui_state);
        hasciicam_gui_state_set_capture_info(&gui_state, cap_info);
        control_count = hasciicam_session_list_controls(&session, control_descs, CAPTURE_MAX_CONTROLS);
        hasciicam_gui_state_set_capture_controls(&gui_state, control_descs, control_count);
        if (loading_image) {
          strncpy(appcfg.image, gui_state.image_path, sizeof(appcfg.image) - 1);
          appcfg.image[sizeof(appcfg.image) - 1] = '\0';
          hasciicam_gui_state_set_source(&gui_state, HASCIICAM_GUI_SOURCE_IMAGE,
                                         "Image",
                                         display_resize_ok ? "image loaded" :
                                                             "image loaded; display resize failed",
                                         display_resize_ok ? 0 : 1);
        } else {
          appcfg.image[0] = '\0';
          hasciicam_gui_state_set_source(&gui_state, HASCIICAM_GUI_SOURCE_CAMERA,
                                         "Camera",
                                         display_resize_ok ? "camera active" :
                                                             "camera active; display resize failed",
                                         display_resize_ok ? 0 : 1);
        }
      }
    }

    if (gui_state.save_requested) {
      gui_state.save_requested = 0;
      hasciicam_gui_state_copy_to_config(&gui_state, &appcfg);
      cfg_err[0] = '\0';
      if (hasciicam_config_save_toml(&appcfg, gui_state.save_path, cfg_err, sizeof(cfg_err))) {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "saved: %s", gui_state.save_path);
        gui_state.status_is_error = 0;
      } else {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "save failed: %s", cfg_err[0] ? cfg_err : "unknown error");
        gui_state.status_is_error = 1;
      }
    }

    if (gui_state.load_requested) {
      char loaded_path[260];
      char saved_path[260];
      hasciicam_config loaded_cfg;
      capture_request loaded_camera_req;
      capture_request requested;
      gui_state.load_requested = 0;
      strncpy(loaded_path, gui_state.load_path, sizeof(loaded_path) - 1);
      loaded_path[sizeof(loaded_path) - 1] = '\0';
      strncpy(saved_path, gui_state.save_path, sizeof(saved_path) - 1);
      saved_path[sizeof(saved_path) - 1] = '\0';
      cfg_err[0] = '\0';
      loaded_cfg = appcfg;
      if (hasciicam_config_load_toml(&loaded_cfg, gui_state.load_path, cfg_err, sizeof(cfg_err))) {
        memset(&loaded_camera_req, 0, sizeof(loaded_camera_req));
        loaded_camera_req.device = loaded_cfg.device;
        loaded_camera_req.input_channel = loaded_cfg.input_channel;
        loaded_camera_req.requested_width = camera_req.requested_width;
        loaded_camera_req.requested_height = camera_req.requested_height;
        requested = loaded_camera_req;
        requested.image_path = loaded_cfg.image;
        if (!hasciicam_session_replace(&session, &requested)) {
          hasciicam_gui_state_set_source(&gui_state, gui_state.source_kind,
                                         gui_state.source_label, capture_last_error(), 1);
          continue;
        }
        appcfg = loaded_cfg;
        camera_req = loaded_camera_req;
        char previous_font[64];
        strncpy(previous_font, gui_state.active_font, sizeof(previous_font) - 1);
        previous_font[sizeof(previous_font) - 1] = '\0';
        hasciicam_gui_state_reset_preview(&gui_state);
        hasciicam_gui_state_init(&gui_state, &appcfg);
        strncpy(gui_state.load_path, loaded_path, sizeof(gui_state.load_path) - 1);
        gui_state.load_path[sizeof(gui_state.load_path) - 1] = '\0';
        strncpy(gui_state.save_path, saved_path, sizeof(gui_state.save_path) - 1);
        gui_state.save_path[sizeof(gui_state.save_path) - 1] = '\0';
        cap_info = hasciicam_session_capture_info(&session);
        hasciicam_gui_state_set_capture_info(&gui_state, cap_info);
        control_count = hasciicam_session_list_controls(&session, control_descs, CAPTURE_MAX_CONTROLS);
        hasciicam_gui_state_set_capture_controls(&gui_state, control_descs, control_count);
        sync_virtual_camera_gui_state(&gui_state, &virtual_camera);
        if (strcmp(previous_font, gui_state.font) != 0)
          gui_state.font_change_requested = 1;
        strncpy(gui_state.active_font, previous_font, sizeof(gui_state.active_font) - 1);
        gui_state.active_font[sizeof(gui_state.active_font) - 1] = '\0';
        hasciicam_gui_state_set_source(&gui_state,
                                       appcfg.image[0] ? HASCIICAM_GUI_SOURCE_IMAGE : HASCIICAM_GUI_SOURCE_CAMERA,
                                       appcfg.image[0] ? "Image" : "Camera",
                                       "configuration loaded", 0);
      } else {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "load failed: %s", cfg_err[0] ? cfg_err : "unknown error");
        gui_state.status_is_error = 1;
      }
    }

    hasciicam_live_controls_apply(&render_session, &session, &appcfg, &gui_state);
    if (gui_state.capture_control_change_requested) {
      gui_state.capture_control_change_requested = 0;
      if (gui_state.capture_control_change_is_auto) {
        if (!hasciicam_session_set_control_auto(&session,
                                                gui_state.capture_control_change_id,
                                                gui_state.capture_control_change_value)) {
          snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                   "camera auto change failed");
          gui_state.status_is_error = 1;
        }
      } else {
        if (!hasciicam_session_set_control(&session,
                                           gui_state.capture_control_change_id,
                                           gui_state.capture_control_change_value)) {
          snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                   "camera control change failed");
          gui_state.status_is_error = 1;
        }
      }
      control_count = hasciicam_session_list_controls(&session, control_descs, CAPTURE_MAX_CONTROLS);
      hasciicam_gui_state_set_capture_controls(&gui_state, control_descs, control_count);
    }
    if (gui_state.font_change_requested) {
      gui_state.font_change_requested = 0;
      if (hasciicam_sdl_set_runtime_font(render_session.context, gui_state.font)) {
        strncpy(gui_state.active_font, gui_state.font, sizeof(gui_state.active_font) - 1);
        gui_state.active_font[sizeof(gui_state.active_font) - 1] = '\0';
      } else {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "font change failed: %s", gui_state.font);
        gui_state.status_is_error = 1;
      }
    }
    hasciicam_sdl_set_runtime_colors(render_session.context,
                                     gui_state.foreground_rgb,
                                     gui_state.background_rgb,
                                     gui_state.aa_dimmer);

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
      if (gui_state.visible)
        hasciicam_gui_state_update_preview(&gui_state, gray_frame, ascii_width, ascii_height);
      dest_size = aa_imgwidth(render_session.context) * aa_imgheight(render_session.context);
      copy_size = (gray_size < dest_size) ? gray_size : dest_size;
      if (copy_size > 0) {
        memcpy(aa_image(render_session.context), gray_frame, copy_size);
        hasciicam_output_write_ascii_frame_tuned(&output, ascii_width, ascii_height,
                                                render_session.render_params);
        frame_rendered = 1;
      }
    }
	/*aa_setpalette (gamma di colori, indice, colore rosso, verde, blu)*/

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//    aa_render (ascii_context, ascii_rndparms, 0, 0,
//	       vid_geo.w,vid_geo.h);

    if (mode == HTML && frame_rendered) {
      hasciicam_output_file_publish_html(aatmpfile, aafile);
    }
    if (frame_rendered && gui_state.save_text_frame_requested) {
      hasciicam_ascii_frame text_frame;
      gui_state.save_text_frame_requested = 0;
      cfg_err[0] = '\0';
      if (hasciicam_render_session_get_ascii_frame(&render_session, &text_frame) &&
          hasciicam_output_text_frame_write(&text_frame, gui_state.text_frame_path,
                                            cfg_err, sizeof(cfg_err))) {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "text frame saved: %s", gui_state.text_frame_path);
        gui_state.status_is_error = 0;
      } else {
        snprintf(gui_state.status_message, sizeof(gui_state.status_message),
                 "text frame save failed: %s", cfg_err[0] ? cfg_err : "rendered frame unavailable");
        gui_state.status_is_error = 1;
      }
    }
    if (frame_rendered) {
      rendered_frames++;
      if (max_frames > 0 && rendered_frames >= max_frames) {
        hasciicam_session_request_stop(&session);
      }
    }
    if (mode == LIVE && appcfg.virtual_camera) {
      sync_virtual_camera_gui_state(&gui_state, &virtual_camera);
    }

  }

  /* CLEAN EXIT */

  hasciicam_session_stop(&session);
  hasciicam_gui_state_reset_preview(&gui_state);
  hasciicam_output_close(&output);
  if (!quiet && mode == LIVE && appcfg.virtual_camera) {
    fprintf(stderr, "Virtual camera frames: accepted=%llu dropped=%llu\n",
            hasciicam_app_virtual_camera_accepted_frames(&virtual_camera),
            hasciicam_app_virtual_camera_dropped_frames(&virtual_camera));
  }
  hasciicam_app_virtual_camera_stop(&virtual_camera, hasciicam_sdl_set_frame_callback);
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
