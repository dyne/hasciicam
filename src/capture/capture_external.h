#ifndef HASCIICAM_CAPTURE_EXTERNAL_H
#define HASCIICAM_CAPTURE_EXTERNAL_H

#include "capture.h"

const capture_ops *capture_external_ops(void);
int capture_external_submit_frame(const unsigned char *data,
                                  size_t data_size,
                                  int width,
                                  int height,
                                  int stride_bytes,
                                  capture_pixel_format pixel_format);

#endif
