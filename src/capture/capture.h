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
    CAPTURE_PIXFMT_NV21,
    CAPTURE_PIXFMT_RGB24,
    CAPTURE_PIXFMT_BGR24,
    CAPTURE_PIXFMT_RGB32,
    CAPTURE_PIXFMT_BGRA32
} capture_pixel_format;

typedef struct capture_request {
    /* Device selector semantics are backend-specific (path, name matcher, etc). */
    const char *device;
    /* Optional source input selector; unsupported values are adapter-defined. */
    int input_channel;
    /* Optional preferred dimensions; adapters may negotiate different values. */
    int requested_width;
    int requested_height;
} capture_request;

typedef struct capture_frame {
    /*
     * Frame memory owned by the backend.
     * The caller must treat this pointer as read-only and call release()
     * exactly once for each successful read().
     */
    const unsigned char *data;
    /* Number of bytes available at data. */
    size_t data_size;
    /* Actual frame dimensions reported by the backend for this frame. */
    int width;
    int height;
    /*
     * Byte distance between two adjacent source rows.
     * May be >= logical row bytes because of padding.
     */
    int stride_bytes;
    /* Pixel format for conversion in frame_convert.c. */
    capture_pixel_format pixel_format;
} capture_frame;

typedef struct capture_info {
    /* Stream-level dimensions negotiated during open/start. */
    int width;
    int height;
    /* Stream-level row stride in bytes for the negotiated pixel format. */
    int stride_bytes;
    /* Stream-level pixel format. */
    capture_pixel_format pixel_format;
} capture_info;

typedef enum capture_control_id {
    CAPTURE_CONTROL_BRIGHTNESS = 0,
    CAPTURE_CONTROL_CONTRAST,
    CAPTURE_CONTROL_GAMMA,
    CAPTURE_CONTROL_GAIN,
    CAPTURE_CONTROL_SATURATION,
    CAPTURE_CONTROL_SHARPNESS,
    CAPTURE_CONTROL_EXPOSURE,
    CAPTURE_CONTROL_WHITE_BALANCE,
    CAPTURE_CONTROL_FOCUS
} capture_control_id;

typedef struct capture_control_desc {
    capture_control_id id;
    const char *name;
    const char *label;
    int min_value;
    int max_value;
    int step;
    int default_value;
    int current_value;
    int writable;
    int auto_supported;
    int auto_enabled;
} capture_control_desc;

typedef struct capture_device capture_device;

typedef struct capture_ops {
    /* Open backend and allocate device state. */
    int (*open)(capture_device **out, const capture_request *req);
    /* Describe currently negotiated stream properties. */
    int (*describe)(capture_device *dev, capture_info *info);
    /* Start frame production after open(). */
    int (*start)(capture_device *dev);
    /* Read one frame; on success caller must call release(). */
    int (*read)(capture_device *dev, capture_frame *frame);
    /* Release frame resources obtained by read(). */
    void (*release)(capture_device *dev, capture_frame *frame);
    /* Stop frame production; idempotent when possible. */
    void (*stop)(capture_device *dev);
    /* Close backend and free device state. */
    void (*close)(capture_device *dev);
    /* Stable backend identifier for diagnostics. */
    const char *(*name)(void);
    /* Optional: enumerate supported controls. Returns count written to out[]. */
    int (*list_controls)(capture_device *dev, capture_control_desc *out, int max_controls);
    /* Optional: set a control value. */
    int (*set_control)(capture_device *dev, capture_control_id id, int value);
    /* Optional: set control auto mode (enabled=0/1). */
    int (*set_control_auto)(capture_device *dev, capture_control_id id, int enabled);
} capture_ops;

#endif
