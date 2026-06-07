#ifndef HASCIICAM_VIRTUAL_CAMERA_PIPE_H
#define HASCIICAM_VIRTUAL_CAMERA_PIPE_H

#include <stddef.h>
#include <stdint.h>

#include "../../../virtual_camera/virtual_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HASCIICAM_VIRTUAL_CAMERA_PIPE_MAGIC 0x31435648u /* HVC1 */
#define HASCIICAM_VIRTUAL_CAMERA_PIPE_VERSION 1u
#define HASCIICAM_VIRTUAL_CAMERA_PIPE_MAX_WIDTH 8192
#define HASCIICAM_VIRTUAL_CAMERA_PIPE_MAX_HEIGHT 8192

typedef struct hasciicam_virtual_camera_pipe_frame {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t pixel_format;
    int32_t width;
    int32_t height;
    int32_t stride_bytes;
    uint64_t sequence;
    uint64_t timestamp_100ns;
    uint32_t payload_bytes;
    uint32_t reserved;
} hasciicam_virtual_camera_pipe_frame;

/**
 * Initialize a versioned pipe frame header.
 */
void hasciicam_virtual_camera_pipe_frame_init(hasciicam_virtual_camera_pipe_frame *frame,
                                              hasciicam_virtual_camera_pixel_format pixel_format,
                                              int width,
                                              int height,
                                              int stride_bytes,
                                              uint64_t sequence,
                                              uint64_t timestamp_100ns);

/**
 * Return the payload size implied by the frame header fields.
 */
size_t hasciicam_virtual_camera_pipe_frame_payload_size(const hasciicam_virtual_camera_pipe_frame *frame);

/**
 * Return the exact serialized message size for a validated frame header.
 */
size_t hasciicam_virtual_camera_pipe_frame_message_size(const hasciicam_virtual_camera_pipe_frame *frame);

/**
 * Validate a versioned pipe frame header.
 */
int hasciicam_virtual_camera_pipe_frame_validate(const hasciicam_virtual_camera_pipe_frame *frame,
                                                 char *err,
                                                 size_t err_size);

/**
 * Build a stable per-user/per-camera pipe name from the configured request.
 */
int hasciicam_virtual_camera_pipe_build_name(const hasciicam_virtual_camera_request *request,
                                             char *out,
                                             size_t out_size,
                                             char *err,
                                             size_t err_size);

/**
 * Build a compact registration payload for the source DLL to consume.
 */
int hasciicam_virtual_camera_pipe_build_registration_payload(const hasciicam_virtual_camera_request *request,
                                                             char *out,
                                                             size_t out_size,
                                                             char *err,
                                                             size_t err_size);

/**
 * Build the SDDL used to restrict access to the named pipe.
 */
int hasciicam_virtual_camera_pipe_build_sddl(char *out,
                                             size_t out_size,
                                             char *err,
                                             size_t err_size);

/**
 * Decode one exact pipe message into a validated header and payload view.
 */
int hasciicam_virtual_camera_pipe_decode_message(const void *bytes,
                                                 size_t bytes_size,
                                                 hasciicam_virtual_camera_pipe_frame *header_out,
                                                 const unsigned char **payload_out,
                                                 size_t *payload_size_out,
                                                 char *err,
                                                 size_t err_size);

/**
 * Encode a validated frame header and payload into one exact pipe message.
 */
int hasciicam_virtual_camera_pipe_encode_message(const hasciicam_virtual_camera_pipe_frame *frame,
                                                 const void *payload,
                                                 size_t payload_size,
                                                 void *bytes,
                                                 size_t bytes_size,
                                                 char *err,
                                                 size_t err_size);

#ifdef __cplusplus
}
#endif

#endif
