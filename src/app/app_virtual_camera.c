#include "app_virtual_camera.h"

#include <stdio.h>
#include <string.h>

#include "../gui/gui_bridge.h"
#include "../virtual_camera/virtual_camera.h"

static void app_virtual_camera_frame_callback(void *user_data,
                                              const struct hasciicam_virtual_camera_frame *frame) {
    hasciicam_app_virtual_camera *vc = (hasciicam_app_virtual_camera *)user_data;

    if (vc == NULL || vc->device == NULL || frame == NULL)
        return;
    (void)hasciicam_virtual_camera_publish(vc->device, frame);
}

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    snprintf(err, err_size, "%s", msg != NULL ? msg : "unknown error");
}

void hasciicam_app_virtual_camera_init(hasciicam_app_virtual_camera *vc) {
    if (vc == NULL)
        return;
    memset(vc, 0, sizeof(*vc));
}

int hasciicam_app_virtual_camera_start(hasciicam_app_virtual_camera *vc,
                                       aa_context *context,
                                       const hasciicam_config *cfg,
                                       hasciicam_app_virtual_camera_set_callback_fn set_callback,
                                       char *err,
                                       size_t err_size) {
    hasciicam_virtual_camera_request request;

    if (vc == NULL || cfg == NULL)
        return 0;
    if (!cfg->virtual_camera)
        return 1;
    if (context == NULL) {
        set_error(err, err_size, "virtual camera requires an active SDL render context");
        return 0;
    }
    if (set_callback == NULL) {
        set_error(err, err_size, "virtual camera requires SDL callback registration support");
        return 0;
    }

    hasciicam_virtual_camera_request_init(&request);
    request.enabled = 1;
    request.width = cfg->virtual_camera_width;
    request.height = cfg->virtual_camera_height;
    request.fps = cfg->virtual_camera_fps;
    strncpy(request.device, cfg->virtual_camera_device, sizeof(request.device) - 1);
    request.device[sizeof(request.device) - 1] = '\0';
    if (!hasciicam_virtual_camera_open_default(&vc->device, &request)) {
        set_error(err, err_size, "virtual camera backend unavailable");
        return 0;
    }
    vc->context = context;
    if (!set_callback(context, app_virtual_camera_frame_callback, vc)) {
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
        vc->context = NULL;
        set_error(err, err_size, "failed to register SDL frame callback");
        return 0;
    }
    vc->active = 1;
    return 1;
}

void hasciicam_app_virtual_camera_stop(hasciicam_app_virtual_camera *vc,
                                       hasciicam_app_virtual_camera_set_callback_fn set_callback) {
    if (vc == NULL)
        return;
    if (vc->active && set_callback != NULL && vc->context != NULL) {
        (void)set_callback(vc->context, NULL, NULL);
    }
    if (vc->device != NULL) {
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
    }
    vc->context = NULL;
    vc->active = 0;
}
