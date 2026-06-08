#ifndef HASCIICAM_VIRTUAL_CAMERA_INTERNAL_H
#define HASCIICAM_VIRTUAL_CAMERA_INTERNAL_H

#include "virtual_camera.h"

/**
 * Allocate the shared device wrapper around platform-owned backend state.
 */
hasciicam_virtual_camera_device *hasciicam_virtual_camera_device_create(
    const hasciicam_virtual_camera_ops *ops,
    int supported,
    const char *backend_name,
    void *backend_state,
    char *err,
    size_t err_size);

/**
 * Return platform-owned state stored in the shared device wrapper.
 */
void *hasciicam_virtual_camera_device_state(hasciicam_virtual_camera_device *device);

/**
 * Store a concise backend error in a caller-owned buffer.
 */
void hasciicam_virtual_camera_set_error(char *err, size_t err_size, const char *msg);

#endif
