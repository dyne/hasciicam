#ifndef HASCIICAM_OUTPUT_FILE_H
#define HASCIICAM_OUTPUT_FILE_H

#include "../render/render_session.h"

int hasciicam_output_file_prepare(hasciicam_render_session *render_session,
                                  int mode,
                                  char *aafile,
                                  char *tmpfile,
                                  size_t tmpfile_size);

#endif
