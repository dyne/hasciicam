#include <stdio.h>
#include <string.h>

#include "../src/capture/frame_convert.h"

static int test_gray8_passthrough(void) {
    unsigned char src[4] = {10, 20, 30, 40};
    unsigned char dst[4] = {0, 0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 2;
    frame.pixel_format = CAPTURE_PIXFMT_GRAY8;

    if (!capture_frame_to_gray_scaled(&frame, dst, 2, 2))
        return 0;
    return dst[0] == 10 && dst[1] == 20 && dst[2] == 30 && dst[3] == 40;
}

static int test_yuyv_reads_luma_bytes(void) {
    /* Y0 U Y1 V */
    unsigned char src[4] = {50, 128, 200, 128};
    unsigned char dst[2] = {0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 2;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixel_format = CAPTURE_PIXFMT_YUYV;

    if (!capture_frame_to_gray_scaled(&frame, dst, 2, 1))
        return 0;
    return dst[0] == 50 && dst[1] == 200;
}

static int test_nv12_reads_y_plane(void) {
    unsigned char src[6] = {7, 8, 9, 10, 128, 128};
    unsigned char dst[4] = {0, 0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 2;
    frame.pixel_format = CAPTURE_PIXFMT_NV12;

    if (!capture_frame_to_gray_scaled(&frame, dst, 2, 2))
        return 0;
    return dst[0] == 7 && dst[1] == 8 && dst[2] == 9 && dst[3] == 10;
}

static int test_nv21_reads_y_plane(void) {
    unsigned char src[6] = {11, 12, 13, 14, 128, 128};
    unsigned char dst[4] = {0, 0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 2;
    frame.pixel_format = CAPTURE_PIXFMT_NV21;

    if (!capture_frame_to_gray_scaled(&frame, dst, 2, 2))
        return 0;
    return dst[0] == 11 && dst[1] == 12 && dst[2] == 13 && dst[3] == 14;
}

static int test_gray8_mirror_x(void) {
    unsigned char src[6] = {1, 2, 3, 4, 5, 6};
    unsigned char dst[6] = {0, 0, 0, 0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 3;
    frame.height = 2;
    frame.stride_bytes = 3;
    frame.pixel_format = CAPTURE_PIXFMT_GRAY8;

    if (!capture_frame_to_gray_scaled_mirrored(&frame, dst, 3, 2, 1, 0))
        return 0;
    return dst[0] == 3 && dst[1] == 2 && dst[2] == 1 &&
           dst[3] == 6 && dst[4] == 5 && dst[5] == 4;
}

static int test_gray8_mirror_y(void) {
    unsigned char src[6] = {1, 2, 3, 4, 5, 6};
    unsigned char dst[6] = {0, 0, 0, 0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 3;
    frame.height = 2;
    frame.stride_bytes = 3;
    frame.pixel_format = CAPTURE_PIXFMT_GRAY8;

    if (!capture_frame_to_gray_scaled_mirrored(&frame, dst, 3, 2, 0, 1))
        return 0;
    return dst[0] == 4 && dst[1] == 5 && dst[2] == 6 &&
           dst[3] == 1 && dst[4] == 2 && dst[5] == 3;
}

static int test_bgra_luma_for_red_pixel(void) {
    /* B, G, R, A */
    unsigned char src[4] = {0, 0, 255, 255};
    unsigned char dst[1] = {0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixel_format = CAPTURE_PIXFMT_BGRA32;

    if (!capture_frame_to_gray_scaled(&frame, dst, 1, 1))
        return 0;
    return dst[0] == 76;
}

static int test_rgb32_luma_for_rgba_pixels(void) {
    /* Browser ImageData is R, G, B, A. */
    unsigned char src[12] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255
    };
    unsigned char dst[3] = {0, 0, 0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 3;
    frame.height = 1;
    frame.stride_bytes = 12;
    frame.pixel_format = CAPTURE_PIXFMT_RGB32;

    if (!capture_frame_to_gray_scaled(&frame, dst, 3, 1))
        return 0;
    return dst[0] == 76 && dst[1] == 149 && dst[2] == 28;
}

static int test_invalid_format_fails(void) {
    unsigned char src[1] = {0};
    unsigned char dst[1] = {0};
    capture_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.data = src;
    frame.data_size = sizeof(src);
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 1;
    frame.pixel_format = CAPTURE_PIXFMT_UNKNOWN;
    return capture_frame_to_gray_scaled(&frame, dst, 1, 1) == 0;
}

int main(void) {
    if (!test_gray8_passthrough()) return 1;
    if (!test_yuyv_reads_luma_bytes()) return 1;
    if (!test_nv12_reads_y_plane()) return 1;
    if (!test_nv21_reads_y_plane()) return 1;
    if (!test_gray8_mirror_x()) return 1;
    if (!test_gray8_mirror_y()) return 1;
    if (!test_bgra_luma_for_red_pixel()) return 1;
    if (!test_rgb32_luma_for_rgba_pixels()) return 1;
    if (!test_invalid_format_fails()) return 1;
    printf("frame_convert tests passed\n");
    return 0;
}
