#include "app_virtual_camera.h"

#include <stdio.h>
#include <string.h>

#include "../gui/gui_bridge.h"
#include "../virtual_camera/virtual_camera.h"

static void app_virtual_camera_frame_callback(void *user_data,
                                              const struct hasciicam_virtual_camera_frame *frame) {
    hasciicam_app_virtual_camera *vc = (hasciicam_app_virtual_camera *)user_data;
    unsigned long long delta;

    if (vc == NULL || vc->device == NULL || frame == NULL)
        return;
    if (vc->min_publish_interval_100ns > 0 && vc->last_publish_100ns != 0) {
        delta = frame->timestamp_100ns - vc->last_publish_100ns;
        if (delta < vc->min_publish_interval_100ns) {
            vc->dropped_frames++;
            return;
        }
    }
    (void)hasciicam_virtual_camera_publish(vc->device, frame);
    vc->last_publish_100ns = frame->timestamp_100ns;
    vc->accepted_frames++;
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
#if defined(__linux__)
    if (cfg->device[0] != '\0' && cfg->virtual_camera_device[0] != '\0' &&
        strcmp(cfg->device, cfg->virtual_camera_device) == 0) {
        set_error(err, err_size, "virtual camera device must differ from capture device");
        return 0;
    }
#endif

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
    vc->min_publish_interval_100ns = (cfg->virtual_camera_fps > 0)
        ? (10000000ULL / (unsigned long long)cfg->virtual_camera_fps)
        : 0;
    if (!set_callback(context, app_virtual_camera_frame_callback, vc)) {
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
        vc->context = NULL;
        set_error(err, err_size, "failed to register SDL frame callback");
        return 0;
    }
#ifdef _WIN32
    if (!hasciicam_app_virtual_camera_windows_start(vc, &request, err, err_size)) {
        (void)set_callback(context, NULL, NULL);
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
        vc->context = NULL;
        return 0;
    }
#endif
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
#ifdef _WIN32
    hasciicam_app_virtual_camera_windows_stop(vc);
#endif
    if (vc->device != NULL) {
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
    }
    vc->context = NULL;
    vc->active = 0;
}

unsigned long long hasciicam_app_virtual_camera_accepted_frames(const hasciicam_app_virtual_camera *vc) {
    return vc != NULL ? vc->accepted_frames : 0ULL;
}

unsigned long long hasciicam_app_virtual_camera_dropped_frames(const hasciicam_app_virtual_camera *vc) {
    return vc != NULL ? vc->dropped_frames : 0ULL;
}
