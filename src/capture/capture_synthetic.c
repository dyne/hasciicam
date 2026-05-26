#include "capture_synthetic.h"

#include <stdlib.h>
#include <string.h>

struct capture_device {
    unsigned char *buffer;
    int width;
    int height;
    int stride_bytes;
    int running;
    unsigned int frame_index;
};

static int synthetic_open(capture_device **out, const capture_request *req) {
    capture_device *dev;
    int width;
    int height;
    size_t size;
    if (out == NULL)
        return 0;
    width = (req != NULL && req->requested_width > 0) ? req->requested_width : 640;
    height = (req != NULL && req->requested_height > 0) ? req->requested_height : 480;
    if (width <= 0 || height <= 0)
        return 0;
    dev = (capture_device *)calloc(1, sizeof(*dev));
    if (dev == NULL)
        return 0;
    size = (size_t)width * (size_t)height;
    dev->buffer = (unsigned char *)malloc(size);
    if (dev->buffer == NULL) {
        free(dev);
        return 0;
    }
    dev->width = width;
    dev->height = height;
    dev->stride_bytes = width;
    dev->running = 0;
    dev->frame_index = 0;
    *out = dev;
    return 1;
}

static int synthetic_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL)
        return 0;
    info->width = dev->width;
    info->height = dev->height;
    info->stride_bytes = dev->stride_bytes;
    info->pixel_format = CAPTURE_PIXFMT_GRAY8;
    return 1;
}

static int synthetic_start(capture_device *dev) {
    if (dev == NULL)
        return 0;
    dev->running = 1;
    return 1;
}

static int synthetic_read(capture_device *dev, capture_frame *frame) {
    int x;
    int y;
    if (dev == NULL || frame == NULL || !dev->running)
        return 0;

    for (y = 0; y < dev->height; y++) {
        unsigned char *row = dev->buffer + (y * dev->stride_bytes);
        for (x = 0; x < dev->width; x++) {
            row[x] = (unsigned char)((x + (int)(dev->frame_index * 7u) + (y * 3)) & 0xFF);
        }
    }
    dev->frame_index++;

    frame->data = dev->buffer;
    frame->data_size = (size_t)dev->height * (size_t)dev->stride_bytes;
    frame->width = dev->width;
    frame->height = dev->height;
    frame->stride_bytes = dev->stride_bytes;
    frame->pixel_format = CAPTURE_PIXFMT_GRAY8;
    return 1;
}

static void synthetic_release(capture_device *dev, capture_frame *frame) {
    (void)dev;
    (void)frame;
}

static void synthetic_stop(capture_device *dev) {
    if (dev == NULL)
        return;
    dev->running = 0;
}

static void synthetic_close(capture_device *dev) {
    if (dev == NULL)
        return;
    free(dev->buffer);
    free(dev);
}

static const char *synthetic_name(void) {
    return "synthetic";
}

static const capture_ops ops = {
    synthetic_open,
    synthetic_describe,
    synthetic_start,
    synthetic_read,
    synthetic_release,
    synthetic_stop,
    synthetic_close,
    synthetic_name
};

const capture_ops *capture_synthetic_ops(void) {
    return &ops;
}
