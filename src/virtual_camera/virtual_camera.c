#include "virtual_camera.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    if (msg == NULL)
        msg = "invalid virtual camera request";
    strncpy(err, msg, err_size - 1);
    err[err_size - 1] = '\0';
}

void hasciicam_virtual_camera_request_init(hasciicam_virtual_camera_request *request) {
    if (request == NULL)
        return;
    memset(request, 0, sizeof(*request));
    request->width = 1280;
    request->height = 720;
    request->fps = 30;
}

int hasciicam_virtual_camera_parse_size(const char *text, int *out_w, int *out_h) {
    char *end = NULL;
    long width = 0;
    long height = 0;

    if (text == NULL || out_w == NULL || out_h == NULL || text[0] == '\0')
        return 0;

    width = strtol(text, &end, 10);
    if (end == text || width <= 0 || *end != 'x' && *end != 'X')
        return 0;

    height = strtol(end + 1, &end, 10);
    if (height <= 0 || *end != '\0')
        return 0;
    if (width > INT_MAX || height > INT_MAX)
        return 0;

    *out_w = (int)width;
    *out_h = (int)height;
    return 1;
}

int hasciicam_virtual_camera_request_validate(const hasciicam_virtual_camera_request *request,
                                              char *err,
                                              size_t err_size) {
    if (request == NULL) {
        set_error(err, err_size, "null virtual camera request");
        return 0;
    }

    if (request->enabled) {
        if (request->width <= 0 || request->height <= 0) {
            set_error(err, err_size, "virtual camera size must be positive");
            return 0;
        }
        if ((request->width & 1) != 0 || (request->height & 1) != 0) {
            set_error(err, err_size, "virtual camera size must be even");
            return 0;
        }
        if (request->fps <= 0) {
            set_error(err, err_size, "virtual camera fps must be positive");
            return 0;
        }
    }

    return 1;
}

struct hasciicam_virtual_camera_device {
    const hasciicam_virtual_camera_ops *ops;
    int supported;
    char backend_name[32];
};

static int unsupported_publish(hasciicam_virtual_camera_device *device,
                               const hasciicam_virtual_camera_frame *frame) {
    (void)device;
    (void)frame;
    return 1;
}

static void unsupported_close(hasciicam_virtual_camera_device *device) {
    free(device);
}

static const char *unsupported_name(void) {
    return "unsupported";
}

static const hasciicam_virtual_camera_ops unsupported_ops = {
    unsupported_publish,
    unsupported_close,
    unsupported_name
};

int hasciicam_virtual_camera_open_default(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request) {
    hasciicam_virtual_camera_device *device = NULL;
    char err[128];

    if (out == NULL)
        return 0;
    *out = NULL;

    if (!hasciicam_virtual_camera_request_validate(request, err, sizeof(err)))
        return 0;

    device = (hasciicam_virtual_camera_device *)calloc(1, sizeof(*device));
    if (device == NULL)
        return 0;

    device->ops = &unsupported_ops;
    device->supported = 0;
    strncpy(device->backend_name, unsupported_name(), sizeof(device->backend_name) - 1);
    *out = device;
    return 1;
}

int hasciicam_virtual_camera_publish(hasciicam_virtual_camera_device *device,
                                     const hasciicam_virtual_camera_frame *frame) {
    if (device == NULL || device->ops == NULL || device->ops->publish == NULL)
        return 0;
    return device->ops->publish(device, frame);
}

void hasciicam_virtual_camera_close(hasciicam_virtual_camera_device *device) {
    if (device == NULL)
        return;
    if (device->ops != NULL && device->ops->close != NULL) {
        device->ops->close(device);
        return;
    }
    free(device);
}

int hasciicam_virtual_camera_is_supported(const hasciicam_virtual_camera_device *device) {
    if (device == NULL)
        return 0;
    return device->supported;
}

const char *hasciicam_virtual_camera_backend_name(const hasciicam_virtual_camera_device *device) {
    if (device == NULL || device->backend_name[0] == '\0')
        return "unknown";
    return device->backend_name;
}
