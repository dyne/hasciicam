#include "output_file.h"

#include <stdio.h>

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

int hasciicam_output_file_prepare(hasciicam_render_session *render_session,
                                  int mode,
                                  char *aafile,
                                  char *tmpfile,
                                  size_t tmpfile_size) {
    if (render_session == NULL || aafile == NULL || tmpfile == NULL)
        return 0;
    if (mode != HTML && mode != TEXT)
        return 0;
    hasciicam_render_session_configure_save(render_session, mode, aafile, tmpfile, tmpfile_size);
    return 1;
}

int hasciicam_output_file_publish_html(const char *tmpfile, const char *aafile) {
    if (tmpfile == NULL || aafile == NULL)
        return 0;
#if defined(_WIN32)
    remove(aafile);
#endif
    return rename(tmpfile, aafile) == 0;
}
