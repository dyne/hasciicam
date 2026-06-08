#ifndef HASCIICAM_APP_VIRTUAL_CAMERA_H
#define HASCIICAM_APP_VIRTUAL_CAMERA_H

#include <aalib.h>

#include "app_config.h"
#include "../virtual_camera/virtual_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef struct IMFVirtualCamera IMFVirtualCamera;
#endif

typedef int (*hasciicam_app_virtual_camera_set_callback_fn)(aa_context *context,
                                                            void (*callback)(void *user_data,
                                                                             const struct hasciicam_virtual_camera_frame *frame),
                                                            void *user_data);

typedef struct hasciicam_app_virtual_camera {
    aa_context *context;
    hasciicam_virtual_camera_device *device;
    hasciicam_virtual_camera_request request;
#ifdef _WIN32
    IMFVirtualCamera *virtual_camera;
    int windows_com_initialized;
    int windows_mf_initialized;
#endif
    int active;
    unsigned long long min_publish_interval_100ns;
    unsigned long long last_publish_100ns;
    unsigned long long accepted_frames;
    unsigned long long dropped_frames;
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

/**
 * Return the number of frames accepted by the current publish gate.
 */
unsigned long long hasciicam_app_virtual_camera_accepted_frames(const hasciicam_app_virtual_camera *vc);

/**
 * Return the number of frames dropped by the current publish gate.
 */
unsigned long long hasciicam_app_virtual_camera_dropped_frames(const hasciicam_app_virtual_camera *vc);

/**
 * Return the current virtual-camera request stored by the app controller.
 */
const hasciicam_virtual_camera_request *hasciicam_app_virtual_camera_request(
    const hasciicam_app_virtual_camera *vc);

/**
 * Return the active backend name for diagnostics.
 */
const char *hasciicam_app_virtual_camera_backend_name(const hasciicam_app_virtual_camera *vc);

/**
 * Format a concise diagnostic context for virtual-camera startup errors.
 */
void hasciicam_app_virtual_camera_format_context(const hasciicam_config *cfg,
                                                 char *out,
                                                 size_t out_size);

#ifdef _WIN32
/**
 * Register and start the Windows session virtual camera.
 */
int hasciicam_app_virtual_camera_windows_start(hasciicam_app_virtual_camera *vc,
                                               const hasciicam_virtual_camera_request *request,
                                               char *err,
                                               size_t err_size);

/**
 * Stop and release the Windows session virtual camera.
 */
void hasciicam_app_virtual_camera_windows_stop(hasciicam_app_virtual_camera *vc);
#endif

#ifdef __cplusplus
}
#endif

#endif
