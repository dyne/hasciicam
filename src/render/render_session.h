#ifndef HASCIICAM_RENDER_SESSION_H
#define HASCIICAM_RENDER_SESSION_H

#include <aalib.h>

typedef struct hasciicam_render_session {
    aa_context *context;
    struct aa_hardware_params hwparams;
    struct aa_renderparams *render_params;
    struct aa_savedata save;
    struct aa_format *html_format;
    char html_header[1024];
} hasciicam_render_session;

typedef struct hasciicam_ascii_frame {
    const char *text;
    const char *attrs;
    int width;
    int height;
} hasciicam_ascii_frame;

void hasciicam_render_session_init(hasciicam_render_session *session);
void hasciicam_render_session_configure_geometry(hasciicam_render_session *session,
                                                 int rec_width,
                                                 int rec_height);
void hasciicam_render_session_prepare_html(hasciicam_render_session *session,
                                           int refresh,
                                           const char *aafile,
                                           int linespace,
                                           const char *background,
                                           const char *foreground,
                                           int fontsize,
                                           const char *fontface);
void hasciicam_render_session_configure_save(hasciicam_render_session *session,
                                             int mode,
                                             char *aafile,
                                             char *tmpfile,
                                             size_t tmpfile_size);
int hasciicam_render_session_open(hasciicam_render_session *session,
                                  int mode,
                                  const char *aadriver,
                                  int quiet);
void hasciicam_render_session_apply_tuning(hasciicam_render_session *session,
                                           int bright,
                                           int contrast,
                                           float gamma);
int hasciicam_render_session_get_ascii_frame(const hasciicam_render_session *session,
                                             hasciicam_ascii_frame *frame);
void hasciicam_render_session_close(hasciicam_render_session *session);

#endif
