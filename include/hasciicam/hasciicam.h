#ifndef HASCIICAM_PUBLIC_API_H
#define HASCIICAM_PUBLIC_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hasciicam_instance hasciicam_instance;

typedef enum hasciicam_pixel_format {
    HASCIICAM_PIXFMT_GRAY8 = 0,
    HASCIICAM_PIXFMT_YUYV = 1,
    HASCIICAM_PIXFMT_NV12 = 2,
    HASCIICAM_PIXFMT_NV21 = 3,
    HASCIICAM_PIXFMT_RGB24 = 4,
    HASCIICAM_PIXFMT_BGR24 = 5,
    HASCIICAM_PIXFMT_RGB32 = 6,
    HASCIICAM_PIXFMT_BGRA32 = 7
} hasciicam_pixel_format;

hasciicam_instance *hasciicam_create(void);
void hasciicam_destroy(hasciicam_instance *instance);

int hasciicam_start_external(hasciicam_instance *instance,
                             int camera_width,
                             int camera_height,
                             int ascii_width,
                             int ascii_height);
/* Start an externally-fed session that presents through an AA-lib display driver. */
int hasciicam_start_external_live(hasciicam_instance *instance,
                                  int camera_width,
                                  int camera_height,
                                  int ascii_width,
                                  int ascii_height,
                                  const char *aa_driver);
void hasciicam_stop(hasciicam_instance *instance);

int hasciicam_submit_frame(hasciicam_instance *instance,
                           const unsigned char *data,
                           size_t data_size,
                           int width,
                           int height,
                           int stride_bytes,
                           hasciicam_pixel_format pixel_format);

int hasciicam_render_frame(hasciicam_instance *instance);
int hasciicam_get_ascii_frame(const hasciicam_instance *instance,
                              const char **text,
                              const char **attrs,
                              int *width,
                              int *height);

#ifdef __cplusplus
}
#endif

#endif
