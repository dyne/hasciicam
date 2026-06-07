#ifndef HASCIICAM_VIRTUAL_CAMERA_H
#define HASCIICAM_VIRTUAL_CAMERA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hasciicam_virtual_camera_pixel_format {
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_UNKNOWN = 0,
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32
} hasciicam_virtual_camera_pixel_format;

typedef struct hasciicam_virtual_camera_request {
    int enabled;
    int width;
    int height;
    int fps;
    char device[256];
} hasciicam_virtual_camera_request;

typedef struct hasciicam_virtual_camera_frame {
    const unsigned char *pixels;
    int width;
    int height;
    int stride_bytes;
    hasciicam_virtual_camera_pixel_format pixel_format;
    unsigned long long timestamp_100ns;
} hasciicam_virtual_camera_frame;

typedef struct hasciicam_virtual_camera_device hasciicam_virtual_camera_device;

typedef struct hasciicam_virtual_camera_ops {
    int (*publish)(hasciicam_virtual_camera_device *device,
                   const hasciicam_virtual_camera_frame *frame);
    void (*close)(hasciicam_virtual_camera_device *device);
    const char *(*name)(void);
} hasciicam_virtual_camera_ops;

/**
 * Initialize a virtual-camera request with the project defaults.
 */
void hasciicam_virtual_camera_request_init(hasciicam_virtual_camera_request *request);

/**
 * Parse a size string in WxH form.
 */
int hasciicam_virtual_camera_parse_size(const char *text, int *out_w, int *out_h);

/**
 * Validate a virtual-camera request and describe any error.
 */
int hasciicam_virtual_camera_request_validate(const hasciicam_virtual_camera_request *request,
                                              char *err,
                                              size_t err_size);

/**
 * Open the default virtual-camera backend or a no-op fallback.
 */
int hasciicam_virtual_camera_open_default(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request);

/**
 * Publish one frame to the active backend or discard it in the no-op fallback.
 */
int hasciicam_virtual_camera_publish(hasciicam_virtual_camera_device *device,
                                     const hasciicam_virtual_camera_frame *frame);

/**
 * Close and free a device obtained from open_default().
 */
void hasciicam_virtual_camera_close(hasciicam_virtual_camera_device *device);

/**
 * Return whether the opened backend can actually publish frames.
 */
int hasciicam_virtual_camera_is_supported(const hasciicam_virtual_camera_device *device);

/**
 * Return the backend name for diagnostics.
 */
const char *hasciicam_virtual_camera_backend_name(const hasciicam_virtual_camera_device *device);

#ifdef __cplusplus
}
#endif

#endif
