#ifndef HASCIICAM_CAPTURE_H
#define HASCIICAM_CAPTURE_H

#include <stddef.h>

/* Canonical frame formats exposed by capture backends. */
typedef enum capture_pixel_format {
    CAPTURE_PIXFMT_UNKNOWN = 0,
    CAPTURE_PIXFMT_GRAY8,
    CAPTURE_PIXFMT_YUYV,
    CAPTURE_PIXFMT_YUY2,
    CAPTURE_PIXFMT_NV12,
    CAPTURE_PIXFMT_RGB24,
    CAPTURE_PIXFMT_BGR24,
    CAPTURE_PIXFMT_RGB32,
    CAPTURE_PIXFMT_BGRA32
} capture_pixel_format;

typedef struct capture_request {
    const char *device;
    int input_channel;
    int requested_width;
    int requested_height;
} capture_request;

typedef struct capture_frame {
    const unsigned char *data;
    size_t data_size;
    int width;
    int height;
    int stride_bytes;
    capture_pixel_format pixel_format;
} capture_frame;

typedef struct capture_info {
    int width;
    int height;
    int stride_bytes;
    capture_pixel_format pixel_format;
} capture_info;

typedef struct capture_device capture_device;

typedef struct capture_ops {
    int (*open)(capture_device **out, const capture_request *req);
    int (*describe)(capture_device *dev, capture_info *info);
    int (*start)(capture_device *dev);
    int (*read)(capture_device *dev, capture_frame *frame);
    void (*release)(capture_device *dev, capture_frame *frame);
    void (*stop)(capture_device *dev);
    void (*close)(capture_device *dev);
    const char *(*name)(void);
} capture_ops;

#endif
