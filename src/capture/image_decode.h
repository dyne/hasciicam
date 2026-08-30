#ifndef HASCIICAM_IMAGE_DECODE_H
#define HASCIICAM_IMAGE_DECODE_H

#include <stddef.h>

typedef struct image_decode_result {
    unsigned char *pixels;
    int width;
    int height;
    int stride_bytes;
    size_t size;
    char error[160];
} image_decode_result;

/* Call before first decode and after release; result must be initialized. */
void image_decode_init(image_decode_result *result);
int image_decode_rgb24(const char *path, image_decode_result *result);
void image_decode_release(image_decode_result *result);

#endif
