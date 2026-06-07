#ifndef HASCIICAM_VIRTUAL_CAMERA_SOURCE_H
#define HASCIICAM_VIRTUAL_CAMERA_SOURCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wchar.h>
#include <guiddef.h>
#include <stddef.h>

#include "../../../virtual_camera/virtual_camera.h"

typedef struct hasciicam_virtual_camera_source_media_type {
    hasciicam_virtual_camera_pixel_format pixel_format;
    const wchar_t *subtype_name;
    const GUID *subtype;
    int width;
    int height;
    int fps;
    int stride_bytes;
    size_t frame_bytes;
    unsigned long long sample_duration_100ns;
    unsigned long long average_bitrate;
    int progressive;
    int square_pixels;
    int stream_id;
    int frameserver_shared;
    int framesource_color;
} hasciicam_virtual_camera_source_media_type;

/**
 * Return the CLSID used to register the HasciiCam virtual camera source.
 */
const GUID *hasciicam_virtual_camera_source_clsid(void);

/**
 * Return the CLSID in canonical "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" form.
 */
const wchar_t *hasciicam_virtual_camera_source_clsid_string(void);

/**
 * Return the number of conservative media types advertised by the source.
 */
size_t hasciicam_virtual_camera_source_media_type_count(void);

/**
 * Describe one advertised media type by index.
 */
int hasciicam_virtual_camera_source_media_type_get(size_t index,
                                                   int width,
                                                   int height,
                                                   int fps,
                                                   hasciicam_virtual_camera_source_media_type *out);

#ifdef __cplusplus
}
#endif

#endif
