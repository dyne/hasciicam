#include "render_session.h"

#include <stdio.h>
#include <string.h>

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

static const char *const html_escapes[] = {
    "<", "&lt;", ">", "&gt;", "&", "&amp;", NULL
};

static struct aa_format hasciicam_html_format = {
    79, 25,
    0, 0,
    0,
    AA_NORMAL_MASK | AA_BOLD_MASK | AA_BOLDFONT_MASK,
    NULL,
    "Pure html",
    ".html",
    NULL,
    "</PRE>\n</FONT>\n</BODY>\n</HTML>\n",
    "\n",
    {"%s", "%s", "%s", "%s", "%s"},
    {"", "", "<B>", "", "<B>"},
    {"", "", "</B>", "", "</B>"},
    html_escapes
};

void hasciicam_render_session_init(hasciicam_render_session *session) {
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    memcpy(&session->hwparams, &aa_defparams, sizeof(session->hwparams));
    session->render_params = aa_getrenderparams();

    session->html_format = &hasciicam_html_format;
    session->html_format->head = session->html_header;
}

void hasciicam_render_session_configure_geometry(hasciicam_render_session *session,
                                                 int rec_width,
                                                 int rec_height) {
    if (session == NULL)
        return;
    session->hwparams.font = NULL;
    session->hwparams.width = 0;
    session->hwparams.height = 0;
    session->hwparams.recwidth = rec_width;
    session->hwparams.recheight = rec_height;
}

void hasciicam_render_session_prepare_html(hasciicam_render_session *session,
                                           int refresh,
                                           const char *aafile,
                                           int linespace,
                                           const char *background,
                                           const char *foreground,
                                           int fontsize,
                                           const char *fontface) {
    if (session == NULL)
        return;
    snprintf(session->html_header, sizeof(session->html_header),
             "<HTML>\n <HEAD> <TITLE>wow! (h)ascii 4 the masses!</TITLE>\n"
             "<META HTTP-EQUIV=\"refresh\" CONTENT=\"%u\"; url=\"%s\">\n"
             "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">\n"
             "<STYLE TYPE=\"text/css\">\n"
             "<!--\npre {\nletter-spacing: 1px;\n"
             "layer-background-color: Black;\n"
             "left : auto;\nline-height : %upx;\n}\n-->\n"
             "</STYLE>\n</HEAD>\n<BODY bgcolor=\"#%s\" text=\"#%s\">\n"
             "<FONT SIZE=%u face=\"%s\">\n<PRE>\n",
             refresh, aafile, linespace, background, foreground, fontsize, fontface);
}

void hasciicam_render_session_configure_save(hasciicam_render_session *session,
                                             int mode,
                                             char *aafile,
                                             char *tmpfile,
                                             size_t tmpfile_size) {
    if (session == NULL)
        return;
    if (mode == HTML) {
        snprintf(tmpfile, tmpfile_size, "%s.tmp", aafile);
        session->save.name = tmpfile;
        session->save.format = session->html_format;
        session->save.file = NULL;
    } else if (mode == TEXT) {
        session->save.name = aafile;
        session->save.format = &aa_text_format;
        session->save.file = NULL;
    }
}

int hasciicam_render_session_open(hasciicam_render_session *session,
                                  int mode,
                                  const char *aadriver,
                                  int quiet) {
    if (session == NULL)
        return 0;

    if (mode > 0) {
        session->context = aa_init(&save_d, &session->hwparams, &session->save);
    } else {
        if (aadriver != NULL && strlen(aadriver) > 0) {
            if (!quiet)
                fprintf(stderr, "Driver preference: %s\n", aadriver);
            aa_recommendhidisplay(aadriver);
        } else {
            if (!quiet)
                fprintf(stderr, "Auto-detecting display driver (SDL preferred)\n");
            aa_recommendhidisplay("SDL");
            aa_recommendhidisplay("X11");
            aa_recommendhidisplay("curses");
            aa_recommendhidisplay("linux");
            aa_recommendhidisplay("stdout");
        }
        session->context = aa_autoinit(&session->hwparams);
    }
    return session->context != NULL;
}

void hasciicam_render_session_apply_tuning(hasciicam_render_session *session,
                                           int bright,
                                           int contrast,
                                           float gamma) {
    if (session == NULL || session->render_params == NULL)
        return;
    session->render_params->bright = bright;
    session->render_params->contrast = contrast;
    session->render_params->gamma = gamma;
}

int hasciicam_render_session_get_ascii_frame(const hasciicam_render_session *session,
                                             hasciicam_ascii_frame *frame) {
    if (session == NULL || session->context == NULL || frame == NULL)
        return 0;
    frame->text = aa_text(session->context);
    frame->attrs = aa_attrs(session->context);
    frame->width = aa_scrwidth(session->context);
    frame->height = aa_scrheight(session->context);
    return frame->text != NULL && frame->attrs != NULL;
}

void hasciicam_render_session_close(hasciicam_render_session *session) {
    if (session == NULL)
        return;
    if (session->context != NULL) {
        aa_close(session->context);
        session->context = NULL;
    }
}
