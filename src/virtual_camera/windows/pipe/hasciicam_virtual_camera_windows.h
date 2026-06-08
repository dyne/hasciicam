#ifndef HASCIICAM_VIRTUAL_CAMERA_WINDOWS_H
#define HASCIICAM_VIRTUAL_CAMERA_WINDOWS_H

#include "../../virtual_camera.h"

int hasciicam_virtual_camera_windows_open(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request,
                                          char *err,
                                          size_t err_size);
const char *hasciicam_virtual_camera_windows_name(void);

#endif
