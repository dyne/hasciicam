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

typedef struct hasciicam_virtual_camera_source_config {
    hasciicam_virtual_camera_request request;
    char pipe_name[256];
    char registration_payload[256];
    char pipe_sddl[256];
    hasciicam_virtual_camera_source_media_type media_types[2];
    size_t media_type_count;
} hasciicam_virtual_camera_source_config;

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

/**
 * Build the stable named-pipe path used to bridge frames from HasciiCam.
 */
int hasciicam_virtual_camera_source_pipe_name(const hasciicam_virtual_camera_request *request,
                                              char *out,
                                              size_t out_size,
                                              char *err,
                                              size_t err_size);

/**
 * Build the compact registration payload consumed by the source DLL.
 */
int hasciicam_virtual_camera_source_registration_payload(const hasciicam_virtual_camera_request *request,
                                                         char *out,
                                                         size_t out_size,
                                                         char *err,
                                                         size_t err_size);

/**
 * Build the named-pipe security descriptor in SDDL form.
 */
int hasciicam_virtual_camera_source_pipe_sddl(char *out,
                                              size_t out_size,
                                              char *err,
                                              size_t err_size);

/**
 * Prepare a complete source configuration from a request.
 */
int hasciicam_virtual_camera_source_config_prepare(const hasciicam_virtual_camera_request *request,
                                                   hasciicam_virtual_camera_source_config *out,
                                                   char *err,
                                                   size_t err_size);

/**
 * Return the sample duration for the requested frame rate in 100 ns units.
 */
unsigned long long hasciicam_virtual_camera_source_sample_duration_100ns(int fps);

/**
 * Return the sample timestamp for a zero-based frame sequence.
 */
unsigned long long hasciicam_virtual_camera_source_sample_time_100ns(unsigned long long start_100ns,
                                                                     unsigned long long sequence,
                                                                     int fps);

#ifdef __cplusplus
}
#endif

#endif
