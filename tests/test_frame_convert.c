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
    if (!test_bgra_luma_for_red_pixel()) return 1;
    if (!test_invalid_format_fails()) return 1;
    printf("frame_convert tests passed\n");
    return 0;
}
