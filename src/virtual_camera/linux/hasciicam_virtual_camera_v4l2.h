#ifndef HASCIICAM_VIRTUAL_CAMERA_V4L2_H
#define HASCIICAM_VIRTUAL_CAMERA_V4L2_H

#include <stddef.h>
#include <sys/types.h>

#include "../virtual_camera.h"

int hasciicam_virtual_camera_v4l2_open(hasciicam_virtual_camera_device **out,
                                       const hasciicam_virtual_camera_request *request,
                                       char *err,
                                       size_t err_size);
const char *hasciicam_virtual_camera_v4l2_name(void);
int hasciicam_virtual_camera_v4l2_validate_device_pair(const char *capture_device,
                                                       const char *output_device,
                                                       char *err,
                                                       size_t err_size);

int hasciicam_virtual_camera_v4l2_is_output_capable(unsigned int capabilities);
int hasciicam_virtual_camera_v4l2_describe_error(int errnum,
                                                 const char *operation,
                                                 char *out,
                                                 size_t out_size);
int hasciicam_virtual_camera_v4l2_should_retry_write(int errnum);
int hasciicam_virtual_camera_v4l2_should_drop_frame(int errnum);
int hasciicam_virtual_camera_v4l2_should_disconnect_write(ssize_t written,
                                                          size_t payload_size,
                                                          int errnum);

#endif
