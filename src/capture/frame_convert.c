#include "frame_convert.h"

static unsigned char clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static unsigned char rgb_to_luma(int r, int g, int b) {
    /* Integer BT.601 approximation: 0.299R + 0.587G + 0.114B */
    return clamp_u8((77 * r + 150 * g + 29 * b) >> 8);
}

int capture_frame_to_gray_scaled(const capture_frame *frame,
                                 unsigned char *dst,
                                 int dst_w,
                                 int dst_h) {
    return capture_frame_to_gray_scaled_mirrored(frame, dst, dst_w, dst_h, 0, 0);
}

int capture_frame_to_gray_scaled_mirrored(const capture_frame *frame,
                                         unsigned char *dst,
                                         int dst_w,
                                         int dst_h,
                                         int mirror_x,
                                         int mirror_y) {
    int y;
    int x;
    int src_w;
    int src_h;

    if (frame == 0 || dst == 0 || frame->data == 0)
        return 0;
    if (dst_w <= 0 || dst_h <= 0 || frame->width <= 0 || frame->height <= 0)
        return 0;

    src_w = frame->width;
    src_h = frame->height;

    for (y = 0; y < dst_h; y++) {
        int src_y = (y * src_h) / dst_h;
        if (mirror_y)
            src_y = src_h - 1 - src_y;
        for (x = 0; x < dst_w; x++) {
            int src_x = (x * src_w) / dst_w;
            if (mirror_x)
                src_x = src_w - 1 - src_x;
            unsigned char gray = 0;

            switch (frame->pixel_format) {
            case CAPTURE_PIXFMT_GRAY8: {
                int idx = src_y * frame->stride_bytes + src_x;
                gray = frame->data[idx];
                break;
            }
            case CAPTURE_PIXFMT_YUYV:
            case CAPTURE_PIXFMT_YUY2: {
                int idx = src_y * frame->stride_bytes + (src_x * 2);
                gray = frame->data[idx];
                break;
            }
            case CAPTURE_PIXFMT_NV12: {
                int idx = src_y * frame->stride_bytes + src_x;
                gray = frame->data[idx];
                break;
            }
            case CAPTURE_PIXFMT_NV21: {
                int idx = src_y * frame->stride_bytes + src_x;
                gray = frame->data[idx];
                break;
            }
            case CAPTURE_PIXFMT_RGB24: {
                int idx = src_y * frame->stride_bytes + (src_x * 3);
                gray = rgb_to_luma(frame->data[idx], frame->data[idx + 1], frame->data[idx + 2]);
                break;
            }
            case CAPTURE_PIXFMT_BGR24: {
                int idx = src_y * frame->stride_bytes + (src_x * 3);
                gray = rgb_to_luma(frame->data[idx + 2], frame->data[idx + 1], frame->data[idx]);
                break;
            }
            case CAPTURE_PIXFMT_RGB32: {
                int idx = src_y * frame->stride_bytes + (src_x * 4);
                gray = rgb_to_luma(frame->data[idx], frame->data[idx + 1], frame->data[idx + 2]);
                break;
            }
            case CAPTURE_PIXFMT_BGRA32: {
                int idx = src_y * frame->stride_bytes + (src_x * 4);
                gray = rgb_to_luma(frame->data[idx + 2], frame->data[idx + 1], frame->data[idx]);
                break;
            }
            default:
                return 0;
            }

            dst[y * dst_w + x] = gray;
        }
    }

    return 1;
}
