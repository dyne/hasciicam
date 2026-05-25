#ifndef HASCIICAM_FRAME_CONVERT_H
#define HASCIICAM_FRAME_CONVERT_H

#include "capture.h"

int capture_frame_to_gray_scaled(const capture_frame *frame,
                                 unsigned char *dst,
                                 int dst_w,
                                 int dst_h);

#endif
