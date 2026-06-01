#include "capture_external.h"

#include <stdlib.h>
#include <string.h>

struct capture_device {
    unsigned char *buffer;
    size_t capacity;
    size_t size;
    int width;
    int height;
    int stride_bytes;
    capture_pixel_format pixel_format;
    int running;
    int has_frame;
};

static capture_device *g_external_device = NULL;

static int external_open(capture_device **out, const capture_request *req) {
    capture_device *dev;
    if (out == NULL)
        return 0;
    dev = (capture_device *)calloc(1, sizeof(*dev));
    if (dev == NULL)
        return 0;
    dev->width = (req != NULL && req->requested_width > 0) ? req->requested_width : 640;
    dev->height = (req != NULL && req->requested_height > 0) ? req->requested_height : 480;
    dev->stride_bytes = dev->width;
    dev->pixel_format = CAPTURE_PIXFMT_GRAY8;
    dev->running = 0;
    dev->has_frame = 0;
    g_external_device = dev;
    *out = dev;
    return 1;
}

static int external_describe(capture_device *dev, capture_info *info) {
    if (dev == NULL || info == NULL)
        return 0;
    info->width = dev->width;
    info->height = dev->height;
    info->stride_bytes = dev->stride_bytes;
    info->pixel_format = dev->pixel_format;
    return 1;
}

static int external_start(capture_device *dev) {
    if (dev == NULL)
        return 0;
    dev->running = 1;
    return 1;
}

static int external_read(capture_device *dev, capture_frame *frame) {
    if (dev == NULL || frame == NULL || !dev->running || !dev->has_frame)
        return 0;
    frame->data = dev->buffer;
    frame->data_size = dev->size;
    frame->width = dev->width;
    frame->height = dev->height;
    frame->stride_bytes = dev->stride_bytes;
    frame->pixel_format = dev->pixel_format;
    return 1;
}

static void external_release(capture_device *dev, capture_frame *frame) {
    (void)frame;
    if (dev == NULL)
        return;
    dev->has_frame = 0;
}

static void external_stop(capture_device *dev) {
    if (dev == NULL)
        return;
    dev->running = 0;
}

static void external_close(capture_device *dev) {
    if (dev == NULL)
        return;
    if (g_external_device == dev)
        g_external_device = NULL;
    free(dev->buffer);
    free(dev);
}

static const char *external_name(void) {
    return "external";
}

static const capture_ops ops = {
    external_open,
    external_describe,
    external_start,
    external_read,
    external_release,
    external_stop,
    external_close,
    external_name,
    NULL,
    NULL,
    NULL
};

const capture_ops *capture_external_ops(void) {
    return &ops;
}

int capture_external_submit_frame(const unsigned char *data,
                                  size_t data_size,
                                  int width,
                                  int height,
                                  int stride_bytes,
                                  capture_pixel_format pixel_format) {
    if (g_external_device == NULL || data == NULL || data_size == 0)
        return 0;
    if ((int)g_external_device->capacity < (int)data_size) {
        unsigned char *new_buffer = (unsigned char *)malloc(data_size);
        if (new_buffer == NULL)
            return 0;
        free(g_external_device->buffer);
        g_external_device->buffer = new_buffer;
        g_external_device->capacity = data_size;
    }
    memcpy(g_external_device->buffer, data, data_size);
    g_external_device->size = data_size;
    g_external_device->width = width;
    g_external_device->height = height;
    g_external_device->stride_bytes = stride_bytes;
    g_external_device->pixel_format = pixel_format;
    g_external_device->has_frame = 1;
    return 1;
}
