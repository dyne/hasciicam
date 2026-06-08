#ifdef _WIN32

#include "../src/app/app_virtual_camera.h"

int hasciicam_app_virtual_camera_windows_start(hasciicam_app_virtual_camera *vc,
                                               const hasciicam_virtual_camera_request *request,
                                               char *err,
                                               size_t err_size) {
    (void)vc;
    (void)request;
    (void)err;
    (void)err_size;
    return 1;
}

void hasciicam_app_virtual_camera_windows_stop(hasciicam_app_virtual_camera *vc) {
    (void)vc;
}

#endif
