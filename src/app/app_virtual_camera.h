#ifndef HASCIICAM_APP_VIRTUAL_CAMERA_H
#define HASCIICAM_APP_VIRTUAL_CAMERA_H

#include <aalib.h>

#include "app_config.h"
#include "../virtual_camera/virtual_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*hasciicam_app_virtual_camera_set_callback_fn)(aa_context *context,
                                                            void (*callback)(void *user_data,
                                                                             const struct hasciicam_virtual_camera_frame *frame),
                                                            void *user_data);

typedef struct hasciicam_app_virtual_camera {
    aa_context *context;
    hasciicam_virtual_camera_device *device;
    int active;
} hasciicam_app_virtual_camera;

/**
 * Initialize an app-side virtual-camera controller.
 */
void hasciicam_app_virtual_camera_init(hasciicam_app_virtual_camera *vc);

/**
 * Open the configured virtual camera and register the SDL frame callback.
 */
int hasciicam_app_virtual_camera_start(hasciicam_app_virtual_camera *vc,
                                       aa_context *context,
                                       const hasciicam_config *cfg,
                                       hasciicam_app_virtual_camera_set_callback_fn set_callback,
                                       char *err,
                                       size_t err_size);

/**
 * Unregister the SDL frame callback and close the active virtual camera.
 */
void hasciicam_app_virtual_camera_stop(hasciicam_app_virtual_camera *vc,
                                       hasciicam_app_virtual_camera_set_callback_fn set_callback);

#ifdef __cplusplus
}
#endif

#endif
