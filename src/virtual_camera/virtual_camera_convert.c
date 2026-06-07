#include "virtual_camera.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int clamp_i(int value, int min_value, int max_value) {
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static unsigned char clamp_u8(int value) {
    return (unsigned char)clamp_i(value, 0, 255);
}

static int align_even(int value) {
    return value & ~1;
}

static int fit_scaled_size(int src_width, int src_height, int dst_width, int dst_height,
                           int *out_w, int *out_h) {
    long long scaled_w;
    long long scaled_h;

    if (src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0)
        return 0;

    scaled_w = (long long)dst_width;
    scaled_h = (scaled_w * src_height) / src_width;
    if (scaled_h > dst_height) {
        scaled_h = dst_height;
        scaled_w = (scaled_h * src_width) / src_height;
    }
    if (scaled_w <= 0 || scaled_h <= 0)
        return 0;

    *out_w = (int)scaled_w;
    *out_h = (int)scaled_h;
    return 1;
}

static int fit_offset(int outer, int inner) {
    int offset = (outer - inner) / 2;
    return offset < 0 ? 0 : offset;
}

static int sample_coord(int out_coord, int out_size, int scaled_size, int mirror) {
    int source = (out_coord * scaled_size) / out_size;
    if (source >= scaled_size)
        source = scaled_size - 1;
    if (mirror)
        source = scaled_size - 1 - source;
    return source;
}

static void bgra_to_yuv(int b, int g, int r, int *y, int *u, int *v) {
    int yy = (66 * r + 129 * g + 25 * b + 128) >> 8;
    int uu = (-38 * r - 74 * g + 112 * b + 128) >> 8;
    int vv = (112 * r - 94 * g - 18 * b + 128) >> 8;
    *y = clamp_i(yy + 16, 16, 235);
    *u = clamp_i(uu + 128, 16, 240);
    *v = clamp_i(vv + 128, 16, 240);
}

size_t hasciicam_virtual_camera_yuy2_size(int width, int height, int stride_bytes) {
    size_t rows;
    size_t stride;

    if (width <= 0 || height <= 0)
        return 0;
    stride = (stride_bytes > 0) ? (size_t)stride_bytes : (size_t)width * 2u;
    rows = (size_t)height;
    if (stride > SIZE_MAX / rows)
        return 0;
    return stride * rows;
}

size_t hasciicam_virtual_camera_nv12_size(int width, int height, int y_stride_bytes, int uv_stride_bytes) {
    size_t y_size;
    size_t uv_size;
    size_t y_stride;
    size_t uv_stride;
    size_t uv_rows;

    if (width <= 0 || height <= 0)
        return 0;
    y_stride = (y_stride_bytes > 0) ? (size_t)y_stride_bytes : (size_t)width;
    uv_stride = (uv_stride_bytes > 0) ? (size_t)uv_stride_bytes : (size_t)width;
    uv_rows = (size_t)align_even(height) / 2u;
    if (y_stride > SIZE_MAX / (size_t)height)
        return 0;
    y_size = y_stride * (size_t)height;
    if (uv_rows > 0 && uv_stride > SIZE_MAX / uv_rows)
        return 0;
    uv_size = uv_stride * uv_rows;
    if (y_size > SIZE_MAX - uv_size)
        return 0;
    return y_size + uv_size;
}

int hasciicam_virtual_camera_scale_bgra32_to_yuy2(const unsigned char *src,
                                                  int src_width,
                                                  int src_height,
                                                  int src_stride_bytes,
                                                  unsigned char *dst,
                                                  int dst_width,
                                                  int dst_height,
                                                  int dst_stride_bytes,
                                                  int mirror_x,
                                                  int mirror_y) {
    int scaled_w = 0;
    int scaled_h = 0;
    int x0;
    int y0;
    int x;
    int y;
    size_t dst_stride;

    if (src == NULL || dst == NULL || src_width <= 0 || src_height <= 0 ||
        dst_width <= 0 || dst_height <= 0 || src_stride_bytes <= 0 || dst_stride_bytes < 0)
        return 0;
    if (!fit_scaled_size(src_width, src_height, dst_width, dst_height, &scaled_w, &scaled_h))
        return 0;

    x0 = fit_offset(dst_width, scaled_w);
    y0 = fit_offset(dst_height, scaled_h);
    dst_stride = (dst_stride_bytes > 0) ? (size_t)dst_stride_bytes : (size_t)dst_width * 2u;
    memset(dst, 0x10, hasciicam_virtual_camera_yuy2_size(dst_width, dst_height, (int)dst_stride));

    for (y = 0; y < scaled_h; ++y) {
        int dst_y = y0 + y;
        unsigned char *dst_row = dst + (size_t)dst_y * dst_stride;
        int src_y = sample_coord(y, scaled_h, src_height, mirror_y);
        for (x = 0; x < scaled_w; x += 2) {
            int x_b = (x + 1 < scaled_w) ? (x + 1) : x;
            int src_x0 = sample_coord(x, scaled_w, src_width, mirror_x);
            int src_x1 = sample_coord(x_b, scaled_w, src_width, mirror_x);
            const unsigned char *p0 = src + (size_t)src_y * (size_t)src_stride_bytes + (size_t)src_x0 * 4u;
            const unsigned char *p1 = src + (size_t)src_y * (size_t)src_stride_bytes + (size_t)src_x1 * 4u;
            int y0v, u0, v0, y1v, u1, v1;
            int u_avg;
            int v_avg;
            int dst_x = x0 + x;
            bgra_to_yuv(p0[0], p0[1], p0[2], &y0v, &u0, &v0);
            bgra_to_yuv(p1[0], p1[1], p1[2], &y1v, &u1, &v1);
            u_avg = (u0 + u1) / 2;
            v_avg = (v0 + v1) / 2;
            dst_row[(size_t)dst_x * 2u + 0u] = clamp_u8(y0v);
            dst_row[(size_t)dst_x * 2u + 1u] = clamp_u8(u_avg);
            if (dst_x + 1 < dst_width) {
                dst_row[(size_t)dst_x * 2u + 2u] = clamp_u8(y1v);
                dst_row[(size_t)dst_x * 2u + 3u] = clamp_u8(v_avg);
            }
        }
    }

    return 1;
}

int hasciicam_virtual_camera_scale_bgra32_to_nv12(const unsigned char *src,
                                                  int src_width,
                                                  int src_height,
                                                  int src_stride_bytes,
                                                  unsigned char *dst,
                                                  int dst_width,
                                                  int dst_height,
                                                  int y_stride_bytes,
                                                  int uv_stride_bytes,
                                                  int mirror_x,
                                                  int mirror_y) {
    int scaled_w = 0;
    int scaled_h = 0;
    int x0;
    int y0;
    int x;
    int y;
    size_t y_stride;
    size_t uv_stride;
    unsigned char *y_plane;
    unsigned char *uv_plane;

    if (src == NULL || dst == NULL || src_width <= 0 || src_height <= 0 ||
        dst_width <= 0 || dst_height <= 0 || src_stride_bytes <= 0 ||
        y_stride_bytes < 0 || uv_stride_bytes < 0)
        return 0;
    if (!fit_scaled_size(src_width, src_height, dst_width, dst_height, &scaled_w, &scaled_h))
        return 0;
    if ((dst_width & 1) != 0 || (dst_height & 1) != 0)
        return 0;

    x0 = fit_offset(dst_width, scaled_w);
    y0 = fit_offset(dst_height, scaled_h);
    y_stride = (y_stride_bytes > 0) ? (size_t)y_stride_bytes : (size_t)dst_width;
    uv_stride = (uv_stride_bytes > 0) ? (size_t)uv_stride_bytes : (size_t)dst_width;
    y_plane = dst;
    uv_plane = dst + y_stride * (size_t)dst_height;
    memset(dst, 0x10, hasciicam_virtual_camera_nv12_size(dst_width, dst_height, (int)y_stride, (int)uv_stride));

    for (y = 0; y < scaled_h; ++y) {
        int dst_y = y0 + y;
        int src_y = sample_coord(y, scaled_h, src_height, mirror_y);
        unsigned char *row = y_plane + (size_t)dst_y * y_stride;
        for (x = 0; x < scaled_w; ++x) {
            int src_x = sample_coord(x, scaled_w, src_width, mirror_x);
            const unsigned char *p = src + (size_t)src_y * (size_t)src_stride_bytes + (size_t)src_x * 4u;
            int yy, u, v;
            bgra_to_yuv(p[0], p[1], p[2], &yy, &u, &v);
            row[x0 + x] = clamp_u8(yy);
        }
    }

    for (y = 0; y < scaled_h; y += 2) {
        int dst_y = y0 + y;
        int src_y0 = sample_coord(y, scaled_h, src_height, mirror_y);
        int src_y1 = sample_coord((y + 1 < scaled_h) ? (y + 1) : y, scaled_h, src_height, mirror_y);
        unsigned char *row = uv_plane + (size_t)(dst_y / 2) * uv_stride;
        for (x = 0; x < scaled_w; x += 2) {
            int src_x0 = sample_coord(x, scaled_w, src_width, mirror_x);
            int src_x1 = sample_coord((x + 1 < scaled_w) ? (x + 1) : x, scaled_w, src_width, mirror_x);
            const unsigned char *p00 = src + (size_t)src_y0 * (size_t)src_stride_bytes + (size_t)src_x0 * 4u;
            const unsigned char *p01 = src + (size_t)src_y0 * (size_t)src_stride_bytes + (size_t)src_x1 * 4u;
            const unsigned char *p10 = src + (size_t)src_y1 * (size_t)src_stride_bytes + (size_t)src_x0 * 4u;
            const unsigned char *p11 = src + (size_t)src_y1 * (size_t)src_stride_bytes + (size_t)src_x1 * 4u;
            int ytmp, u00, v00, u01, v01, u10, v10, u11, v11;
            int u_avg;
            int v_avg;
            bgra_to_yuv(p00[0], p00[1], p00[2], &ytmp, &u00, &v00);
            bgra_to_yuv(p01[0], p01[1], p01[2], &ytmp, &u01, &v01);
            bgra_to_yuv(p10[0], p10[1], p10[2], &ytmp, &u10, &v10);
            bgra_to_yuv(p11[0], p11[1], p11[2], &ytmp, &u11, &v11);
            u_avg = (u00 + u01 + u10 + u11) / 4;
            v_avg = (v00 + v01 + v10 + v11) / 4;
            row[x0 + x] = clamp_u8(u_avg);
            row[x0 + x + 1] = clamp_u8(v_avg);
        }
    }

    return 1;
}
