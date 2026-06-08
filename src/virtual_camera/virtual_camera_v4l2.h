#ifndef HASCIICAM_VIRTUAL_CAMERA_V4L2_H
#define HASCIICAM_VIRTUAL_CAMERA_V4L2_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__)
/**
 * Return whether V4L2 capabilities describe an output-capable device.
 */
int hasciicam_virtual_camera_v4l2_is_output_capable(unsigned int capabilities);
#endif

#ifdef __cplusplus
}
#endif

#endif
