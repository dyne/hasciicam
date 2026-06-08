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
    if (vc->last_publish_100ns == 0 || vc->min_publish_interval_100ns == 0) {
        vc->last_publish_100ns = frame->timestamp_100ns;
    } else {
        do {
            vc->last_publish_100ns += vc->min_publish_interval_100ns;
        } while (frame->timestamp_100ns - vc->last_publish_100ns >=
                 vc->min_publish_interval_100ns);
    }
    vc->accepted_frames++;
}

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    snprintf(err, err_size, "%s", msg != NULL ? msg : "unknown error");
}

void hasciicam_app_virtual_camera_format_context(const hasciicam_config *cfg,
                                                 char *out,
                                                 size_t out_size) {
    const char *device = "";
    const char *backend = "";
    int width = 0;
    int height = 0;
    int fps = 0;

    if (out == NULL || out_size == 0)
        return;
    if (cfg != NULL) {
        device = cfg->virtual_camera_device;
        width = cfg->virtual_camera_width;
        height = cfg->virtual_camera_height;
        fps = cfg->virtual_camera_fps;
    }
    backend = hasciicam_virtual_camera_default_backend_name();
    snprintf(out, out_size, "backend=%s size=%dx%d fps=%d device=%s",
             backend,
             width,
             height,
             fps,
             (device != NULL && device[0] != '\0') ? device : "(default)");
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
    if (!hasciicam_virtual_camera_validate_device_pair(cfg->device,
                                                       cfg->virtual_camera_device,
                                                       err,
                                                       err_size)) {
        return 0;
    }

    hasciicam_virtual_camera_request_init(&request);
    request.enabled = 1;
    request.width = cfg->virtual_camera_width;
    request.height = cfg->virtual_camera_height;
    request.fps = cfg->virtual_camera_fps;
    strncpy(request.device, cfg->virtual_camera_device, sizeof(request.device) - 1);
    request.device[sizeof(request.device) - 1] = '\0';
    vc->request = request;
    if (!hasciicam_virtual_camera_open_default(&vc->device, &request, err, err_size)) {
        char context[256];
        hasciicam_app_virtual_camera_format_context(cfg, context, sizeof(context));
        if (context[0] != '\0') {
            char detail[320];
            snprintf(detail, sizeof(detail), "virtual camera backend unavailable (%s)", context);
            set_error(err, err_size, detail);
        } else {
            set_error(err, err_size, "virtual camera backend unavailable");
        }
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
    if (vc->device != NULL) {
        hasciicam_virtual_camera_close(vc->device);
        vc->device = NULL;
    }
#ifdef _WIN32
    hasciicam_app_virtual_camera_windows_stop(vc);
#endif
    vc->context = NULL;
    vc->active = 0;
}

unsigned long long hasciicam_app_virtual_camera_accepted_frames(const hasciicam_app_virtual_camera *vc) {
    return vc != NULL ? vc->accepted_frames : 0ULL;
}

unsigned long long hasciicam_app_virtual_camera_dropped_frames(const hasciicam_app_virtual_camera *vc) {
    return vc != NULL ? vc->dropped_frames : 0ULL;
}

const hasciicam_virtual_camera_request *hasciicam_app_virtual_camera_request(
    const hasciicam_app_virtual_camera *vc) {
    return vc != NULL ? &vc->request : NULL;
}

const char *hasciicam_app_virtual_camera_backend_name(const hasciicam_app_virtual_camera *vc) {
    if (vc == NULL || vc->device == NULL)
        return "";
    return hasciicam_virtual_camera_backend_name(vc->device);
}
