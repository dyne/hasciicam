#ifndef HASCIICAM_VIRTUAL_CAMERA_V4L2_H
#define HASCIICAM_VIRTUAL_CAMERA_V4L2_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__)
#include <sys/types.h>
/**
 * Return whether V4L2 capabilities describe an output-capable device.
 */
int hasciicam_virtual_camera_v4l2_is_output_capable(unsigned int capabilities);

/**
 * Format a concise Linux V4L2 failure message for diagnostics.
 */
int hasciicam_virtual_camera_v4l2_describe_error(int errnum,
                                                 const char *operation,
                                                 char *out,
                                                 size_t out_size);

/**
 * Return whether a write error should be retried without dropping the frame.
 */
int hasciicam_virtual_camera_v4l2_should_retry_write(int errnum);

/**
 * Return whether a write error should be treated as a dropped frame.
 */
int hasciicam_virtual_camera_v4l2_should_drop_frame(int errnum);

/**
 * Return whether a partial write or fatal write error should disconnect the publisher.
 */
int hasciicam_virtual_camera_v4l2_should_disconnect_write(ssize_t written, size_t payload_size, int errnum);
#endif

#ifdef __cplusplus
}
#endif

#endif
