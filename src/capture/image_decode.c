#include "image_decode.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define STBI_NO_BMP
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb/stb_image.h"

static void image_error(image_decode_result *result, const char *message) {
    snprintf(result->error, sizeof(result->error), "%s", message);
}

void image_decode_init(image_decode_result *result) {
    if (result != NULL)
        memset(result, 0, sizeof(*result));
}

void image_decode_release(image_decode_result *result) {
    if (result == NULL)
        return;
    stbi_image_free(result->pixels);
    memset(result, 0, sizeof(*result));
}

int image_decode_rgb24(const char *path, image_decode_result *result) {
    int width;
    int height;
    unsigned char *pixels;
    size_t stride;
    size_t size;
    const char *reason;

    if (result == NULL)
        return 0;
    /* The caller owns any prior storage and must release it before reuse. */
    image_decode_init(result);
    if (path == NULL || path[0] == '\0') {
        image_error(result, "image path is empty");
        return 0;
    }
    pixels = stbi_load(path, &width, &height, NULL, 3);
    if (pixels == NULL) {
        reason = stbi_failure_reason();
        snprintf(result->error, sizeof(result->error), "cannot decode image: %s",
                 reason != NULL ? reason : "unsupported or corrupt input");
        return 0;
    }
    if (width <= 0 || height <= 0 || width > INT_MAX / 3) {
        stbi_image_free(pixels);
        image_error(result, "decoded image has invalid dimensions");
        return 0;
    }
    stride = (size_t)width * 3u;
    if ((size_t)height > SIZE_MAX / stride || stride > (size_t)INT_MAX) {
        stbi_image_free(pixels);
        image_error(result, "decoded image dimensions overflow storage");
        return 0;
    }
    size = stride * (size_t)height;
    result->pixels = pixels;
    result->width = width;
    result->height = height;
    result->stride_bytes = (int)stride;
    result->size = size;
    return 1;
}
