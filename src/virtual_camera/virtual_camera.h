#ifndef HASCIICAM_VIRTUAL_CAMERA_H
#define HASCIICAM_VIRTUAL_CAMERA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hasciicam_virtual_camera_pixel_format {
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_UNKNOWN = 0,
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32,
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
    HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12
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
 * Validate platform-specific capture/output device constraints.
 */
int hasciicam_virtual_camera_validate_device_pair(const char *capture_device,
                                                  const char *output_device,
                                                  char *err,
                                                  size_t err_size);

/**
 * Return the default backend name for startup diagnostics.
 */
const char *hasciicam_virtual_camera_default_backend_name(void);

/**
 * Return the number of bytes needed for a packed YUY2 frame.
 */
size_t hasciicam_virtual_camera_yuy2_size(int width, int height, int stride_bytes);

/**
 * Return the number of bytes needed for an NV12 frame.
 */
size_t hasciicam_virtual_camera_nv12_size(int width, int height, int y_stride_bytes, int uv_stride_bytes);

/**
 * Scale a BGRA32 frame into a YUY2 output buffer using nearest-neighbor sampling and letterboxing.
 */
int hasciicam_virtual_camera_scale_bgra32_to_yuy2(const unsigned char *src,
                                                  int src_width,
                                                  int src_height,
                                                  int src_stride_bytes,
                                                  unsigned char *dst,
                                                  int dst_width,
                                                  int dst_height,
                                                  int dst_stride_bytes,
                                                  int mirror_x,
                                                  int mirror_y);

/**
 * Compute the centered letterbox rectangle for scaling src into dst.
 */
int hasciicam_virtual_camera_letterbox_rect(int src_width,
                                            int src_height,
                                            int dst_width,
                                            int dst_height,
                                            int *out_x,
                                            int *out_y,
                                            int *out_width,
                                            int *out_height);

/**
 * Scale a BGRA32 frame into an NV12 output buffer using nearest-neighbor sampling and letterboxing.
 */
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
                                                  int mirror_y);

/**
 * Open the default virtual-camera backend or a no-op fallback.
 */
int hasciicam_virtual_camera_open_default(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request,
                                          char *err,
                                          size_t err_size);

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
