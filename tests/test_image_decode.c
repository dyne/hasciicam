#include "image_decode.h"
#include <stdio.h>
#include <string.h>

#ifndef HASCIICAM_SOURCE_DIR
#define HASCIICAM_SOURCE_DIR "."
#endif

static int expect_fail(const char *path) {
    image_decode_result result;
    image_decode_init(&result);
    if (image_decode_rgb24(path, &result) || result.pixels != NULL || result.error[0] == '\0') return 0;
    image_decode_release(&result);
    return 1;
}
int main(void) {
    image_decode_result png, jpeg;
    image_decode_init(&png);
    image_decode_init(&jpeg);
    const char *png_path = HASCIICAM_SOURCE_DIR "/tests/fixtures/image-red.png";
    const char *jpeg_path = HASCIICAM_SOURCE_DIR "/tests/fixtures/image-red.jpg";
    if (!image_decode_rgb24(png_path, &png) || png.width != 1 || png.height != 1 || png.stride_bytes != 3 || png.size != 3 || png.pixels[0] < 240 || png.pixels[1] > 15 || png.pixels[2] > 15) return 1;
    if (!image_decode_rgb24(jpeg_path, &jpeg)) return 2;
    if (jpeg.width != 2 || jpeg.height != 2 || jpeg.stride_bytes != 6 || jpeg.pixels[0] < 180 || jpeg.pixels[1] > 80 || jpeg.pixels[2] > 80) return 2;
    image_decode_release(&png);
    if (jpeg.pixels == NULL || jpeg.pixels[0] < 180) return 3;
    image_decode_release(&jpeg);
    if (!image_decode_rgb24(png_path, &png)) return 5;
    image_decode_release(&png);
    if (!expect_fail("") || !expect_fail(HASCIICAM_SOURCE_DIR "/tests/fixtures/missing.png") || !expect_fail(HASCIICAM_SOURCE_DIR "/tests/fixtures/corrupt.bin")) return 4;
    return 0;
}
