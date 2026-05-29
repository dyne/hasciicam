#ifndef HASCIICAM_FRAME_CONVERT_H
#define HASCIICAM_FRAME_CONVERT_H

#include "capture.h"

/*
 * Convert a capture frame into a grayscale image scaled to dst_w x dst_h.
 *
 * Contract:
 * - frame, frame->data, and dst must be non-null.
 * - dst_w, dst_h, frame->width, and frame->height must be > 0.
 * - frame->stride_bytes should be >= logical row bytes for interleaved formats.
 * - supported pixel formats are those handled by frame_convert.c:
 *   GRAY8, YUYV, YUY2, NV12, NV21, RGB24, BGR24, RGB32, BGRA32.
 *
 * Returns:
 * - 1 on successful conversion.
 * - 0 on invalid input or unsupported frame format.
 */
int capture_frame_to_gray_scaled(const capture_frame *frame,
                                 unsigned char *dst,
                                 int dst_w,
                                 int dst_h);

/*
 * Convert and optionally mirror while sampling source pixels.
 *
 * mirror_x flips left/right and mirror_y flips top/bottom. The mirroring is
 * applied in the source-coordinate mapping, so no extra output pass is needed.
 */
int capture_frame_to_gray_scaled_mirrored(const capture_frame *frame,
                                          unsigned char *dst,
                                          int dst_w,
                                          int dst_h,
                                          int mirror_x,
                                          int mirror_y);

#endif
