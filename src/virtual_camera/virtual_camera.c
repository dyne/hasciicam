#include "virtual_camera.h"
#include "virtual_camera_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(_WIN32)
#include "windows/pipe/hasciicam_virtual_camera_windows.h"
#elif defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(__linux__)
#include "linux/hasciicam_virtual_camera_v4l2.h"
#endif

struct hasciicam_virtual_camera_device {
    const hasciicam_virtual_camera_ops *ops;
    int supported;
    char backend_name[32];
    void *backend_state;
};

void hasciicam_virtual_camera_set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    if (msg == NULL)
        msg = "invalid virtual camera request";
    snprintf(err, err_size, "%s", msg);
}

hasciicam_virtual_camera_device *hasciicam_virtual_camera_device_create(
    const hasciicam_virtual_camera_ops *ops,
    int supported,
    const char *backend_name,
    void *backend_state,
    char *err,
    size_t err_size) {
    hasciicam_virtual_camera_device *device;

    device = (hasciicam_virtual_camera_device *)calloc(1, sizeof(*device));
    if (device == NULL) {
        hasciicam_virtual_camera_set_error(err, err_size, "unable to allocate virtual camera device");
        return NULL;
    }
    device->ops = ops;
    device->supported = supported;
    device->backend_state = backend_state;
    snprintf(device->backend_name,
             sizeof(device->backend_name),
             "%s",
             backend_name != NULL ? backend_name : "unknown");
    return device;
}

void *hasciicam_virtual_camera_device_state(hasciicam_virtual_camera_device *device) {
    return device != NULL ? device->backend_state : NULL;
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
    long width;
    long height;

    if (text == NULL || out_w == NULL || out_h == NULL || text[0] == '\0')
        return 0;
    width = strtol(text, &end, 10);
    if (end == text || width <= 0 || (*end != 'x' && *end != 'X'))
        return 0;
    height = strtol(end + 1, &end, 10);
    if (height <= 0 || *end != '\0' || width > INT_MAX || height > INT_MAX)
        return 0;
    *out_w = (int)width;
    *out_h = (int)height;
    return 1;
}

int hasciicam_virtual_camera_request_validate(const hasciicam_virtual_camera_request *request,
                                              char *err,
                                              size_t err_size) {
    if (request == NULL) {
        hasciicam_virtual_camera_set_error(err, err_size, "null virtual camera request");
        return 0;
    }
    if (request->enabled) {
        if (request->width <= 0 || request->height <= 0) {
            hasciicam_virtual_camera_set_error(err, err_size, "virtual camera size must be positive");
            return 0;
        }
        if ((request->width & 1) != 0 || (request->height & 1) != 0) {
            hasciicam_virtual_camera_set_error(err, err_size, "virtual camera size must be even");
            return 0;
        }
        if (request->fps <= 0) {
            hasciicam_virtual_camera_set_error(err, err_size, "virtual camera fps must be positive");
            return 0;
        }
    }
    return 1;
}

int hasciicam_virtual_camera_validate_device_pair(const char *capture_device,
                                                  const char *output_device,
                                                  char *err,
                                                  size_t err_size) {
#if defined(__linux__)
    return hasciicam_virtual_camera_v4l2_validate_device_pair(
        capture_device, output_device, err, err_size);
#else
    (void)capture_device;
    (void)output_device;
    (void)err;
    (void)err_size;
    return 1;
#endif
}

const char *hasciicam_virtual_camera_default_backend_name(void) {
#if defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(_WIN32)
    return hasciicam_virtual_camera_windows_name();
#elif defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(__linux__)
    return hasciicam_virtual_camera_v4l2_name();
#else
    return "unsupported";
#endif
}

static int unsupported_publish(hasciicam_virtual_camera_device *device,
                               const hasciicam_virtual_camera_frame *frame) {
    (void)device;
    (void)frame;
    return 1;
}

static void unsupported_close(hasciicam_virtual_camera_device *device) {
    (void)device;
}

static const char *unsupported_name(void) {
    return "unsupported";
}

static const hasciicam_virtual_camera_ops unsupported_ops = {
    unsupported_publish,
    unsupported_close,
    unsupported_name
};

static int open_unsupported(hasciicam_virtual_camera_device **out,
                            char *err,
                            size_t err_size) {
    *out = hasciicam_virtual_camera_device_create(&unsupported_ops,
                                                  0,
                                                  unsupported_name(),
                                                  NULL,
                                                  err,
                                                  err_size);
    return *out != NULL;
}

int hasciicam_virtual_camera_open_default(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request,
                                          char *err,
                                          size_t err_size) {
    char local_err[128];

    if (out == NULL)
        return 0;
    *out = NULL;
    if (!hasciicam_virtual_camera_request_validate(request, local_err, sizeof(local_err))) {
        hasciicam_virtual_camera_set_error(err, err_size, local_err);
        return 0;
    }
    if (!request->enabled)
        return open_unsupported(out, err, err_size);

#if defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(_WIN32)
    return hasciicam_virtual_camera_windows_open(out, request, err, err_size);
#elif defined(HASCIICAM_ENABLE_VIRTUAL_CAMERA) && defined(__linux__)
    return hasciicam_virtual_camera_v4l2_open(out, request, err, err_size);
#else
    return open_unsupported(out, err, err_size);
#endif
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
    if (device->ops != NULL && device->ops->close != NULL)
        device->ops->close(device);
    free(device);
}

int hasciicam_virtual_camera_is_supported(const hasciicam_virtual_camera_device *device) {
    return device != NULL ? device->supported : 0;
}

const char *hasciicam_virtual_camera_backend_name(const hasciicam_virtual_camera_device *device) {
    if (device == NULL || device->backend_name[0] == '\0')
        return "unknown";
    return device->backend_name;
}
