#ifndef HASCIICAM_OUTPUT_H
#define HASCIICAM_OUTPUT_H

#include <aalib.h>

typedef struct hasciicam_output {
    aa_context *context;
    const char *adapter_name;
} hasciicam_output;

int hasciicam_output_open_aalib(hasciicam_output *output, aa_context *context);
void hasciicam_output_write_ascii_frame(hasciicam_output *output, int ascii_width, int ascii_height);
void hasciicam_output_write_ascii_frame_tuned(hasciicam_output *output,
                                              int ascii_width,
                                              int ascii_height,
                                              const aa_renderparams *render_params);
void hasciicam_output_poll(hasciicam_output *output);
void hasciicam_output_close(hasciicam_output *output);
const char *hasciicam_output_name(const hasciicam_output *output);

#endif
