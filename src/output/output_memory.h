#ifndef HASCIICAM_OUTPUT_MEMORY_H
#define HASCIICAM_OUTPUT_MEMORY_H

#include "../render/render_session.h"

typedef struct hasciicam_memory_output {
    char *text;
    char *attrs;
    int width;
    int height;
    int capacity;
} hasciicam_memory_output;

void hasciicam_output_memory_init(hasciicam_memory_output *output);
int hasciicam_output_memory_write(hasciicam_memory_output *output,
                                  const hasciicam_ascii_frame *frame);
void hasciicam_output_memory_close(hasciicam_memory_output *output);

#endif
