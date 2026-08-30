#ifndef HASCIICAM_OUTPUT_TEXT_FRAME_H
#define HASCIICAM_OUTPUT_TEXT_FRAME_H

#include <stddef.h>

#include "../render/render_session.h"

/* Save the rendered character grid as plain text; AA attributes are ignored. */
int hasciicam_output_text_frame_write(const hasciicam_ascii_frame *frame,
                                      const char *destination,
                                      char *error,
                                      size_t error_size);

#endif
