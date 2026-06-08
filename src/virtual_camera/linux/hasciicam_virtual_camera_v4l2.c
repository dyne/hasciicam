#include "hasciicam_virtual_camera_v4l2.h"

#include "../virtual_camera_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct hasciicam_virtual_camera_v4l2_state {
    int fd;
    int width;
    int height;
    int stride_bytes;
    size_t payload_size;
    unsigned char *payload_buffer;
    int disconnected;
} hasciicam_virtual_camera_v4l2_state;

int hasciicam_virtual_camera_v4l2_is_output_capable(unsigned int capabilities) {
    return (capabilities & V4L2_CAP_VIDEO_OUTPUT) != 0 ||
           (capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) != 0;
}

int hasciicam_virtual_camera_v4l2_describe_error(int errnum,
                                                 const char *operation,
                                                 char *out,
                                                 size_t out_size) {
    const char *reason = NULL;

    if (out == NULL || out_size == 0)
        return 0;
    if (errnum == EACCES || errnum == EPERM)
        reason = "access denied";
    else if (errnum == ENOENT || errnum == ENODEV || errnum == ENXIO)
        reason = "device absent";
    else if (errnum == EBUSY)
        reason = "device busy";
    else if (errnum == EINVAL && operation != NULL && strcmp(operation, "querycap") == 0)
        reason = "device is not a V4L2 output node";
    else if (errnum == EINVAL && operation != NULL && strcmp(operation, "set format") == 0)
        reason = "format rejected";
    else if (errnum == EPIPE || errnum == ENOTCONN)
        reason = "transport disconnected";

    if (reason != NULL)
        snprintf(out, out_size, "virtual camera %s", reason);
    else if (operation != NULL && operation[0] != '\0')
        snprintf(out, out_size, "virtual camera %s failed: %s", operation, strerror(errnum));
    else
        snprintf(out, out_size, "virtual camera operation failed: %s", strerror(errnum));
    return 1;
}

int hasciicam_virtual_camera_v4l2_should_retry_write(int errnum) {
    return errnum == EINTR;
}

int hasciicam_virtual_camera_v4l2_should_drop_frame(int errnum) {
    return errnum == EAGAIN || errnum == EWOULDBLOCK;
}

int hasciicam_virtual_camera_v4l2_should_disconnect_write(ssize_t written,
                                                          size_t payload_size,
                                                          int errnum) {
    if (written == (ssize_t)payload_size)
        return 0;
    if (written < 0 && hasciicam_virtual_camera_v4l2_should_retry_write(errnum))
        return 0;
    if (written < 0 && hasciicam_virtual_camera_v4l2_should_drop_frame(errnum))
        return 0;
    return 1;
}

static int v4l2_publish(hasciicam_virtual_camera_device *device,
                        const hasciicam_virtual_camera_frame *frame) {
    hasciicam_virtual_camera_v4l2_state *state;
    ssize_t written;

    state = (hasciicam_virtual_camera_v4l2_state *)
        hasciicam_virtual_camera_device_state(device);
    if (state == NULL || frame == NULL || state->fd < 0 || state->payload_buffer == NULL)
        return 0;
    if (state->disconnected)
        return 1;
    if (frame->pixel_format != HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32 || frame->pixels == NULL)
        return 0;
    if (!hasciicam_virtual_camera_scale_bgra32_to_yuy2(frame->pixels,
                                                       frame->width,
                                                       frame->height,
                                                       frame->stride_bytes,
                                                       state->payload_buffer,
                                                       state->width,
                                                       state->height,
                                                       state->stride_bytes,
                                                       0,
                                                       0))
        return 0;

    for (;;) {
        written = write(state->fd, state->payload_buffer, state->payload_size);
        if (written == (ssize_t)state->payload_size)
            return 1;
        if (written < 0 && hasciicam_virtual_camera_v4l2_should_retry_write(errno))
            continue;
        if (written < 0 && hasciicam_virtual_camera_v4l2_should_drop_frame(errno))
            return 1;
        if (!hasciicam_virtual_camera_v4l2_should_disconnect_write(
                written, state->payload_size, errno))
            return 1;
        state->disconnected = 1;
        close(state->fd);
        state->fd = -1;
        return 1;
    }
}

static void v4l2_close(hasciicam_virtual_camera_device *device) {
    hasciicam_virtual_camera_v4l2_state *state;

    state = (hasciicam_virtual_camera_v4l2_state *)
        hasciicam_virtual_camera_device_state(device);
    if (state == NULL)
        return;
    if (state->fd >= 0)
        close(state->fd);
    free(state->payload_buffer);
    free(state);
}

const char *hasciicam_virtual_camera_v4l2_name(void) {
    return "v4l2-output";
}

static const hasciicam_virtual_camera_ops v4l2_ops = {
    v4l2_publish,
    v4l2_close,
    hasciicam_virtual_camera_v4l2_name
};

int hasciicam_virtual_camera_v4l2_validate_device_pair(const char *capture_device,
                                                       const char *output_device,
                                                       char *err,
                                                       size_t err_size) {
    if (capture_device != NULL && output_device != NULL &&
        capture_device[0] != '\0' && output_device[0] != '\0' &&
        strcmp(capture_device, output_device) == 0) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "virtual camera device must differ from capture device");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_v4l2_open(hasciicam_virtual_camera_device **out,
                                       const hasciicam_virtual_camera_request *request,
                                       char *err,
                                       size_t err_size) {
    hasciicam_virtual_camera_v4l2_state *state;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;

    if (out == NULL || request == NULL || request->device[0] == '\0')
        return 0;
    *out = NULL;
    state = (hasciicam_virtual_camera_v4l2_state *)calloc(1, sizeof(*state));
    if (state == NULL) {
        hasciicam_virtual_camera_set_error(err, err_size, "unable to allocate V4L2 state");
        return 0;
    }
    state->fd = open(request->device, O_WRONLY | O_NONBLOCK);
    if (state->fd < 0) {
        (void)hasciicam_virtual_camera_v4l2_describe_error(
            errno, "open output device", err, err_size);
        free(state);
        return 0;
    }

    memset(&cap, 0, sizeof(cap));
    if (ioctl(state->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        (void)hasciicam_virtual_camera_v4l2_describe_error(
            errno, "querycap", err, err_size);
        close(state->fd);
        free(state);
        return 0;
    }
    if (!hasciicam_virtual_camera_v4l2_is_output_capable(cap.capabilities)) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "virtual camera device is not a V4L2 output node");
        close(state->fd);
        free(state);
        return 0;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = (unsigned int)request->width;
    fmt.fmt.pix.height = (unsigned int)request->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = (unsigned int)(request->width * 2);
    fmt.fmt.pix.sizeimage = (unsigned int)hasciicam_virtual_camera_yuy2_size(
        request->width, request->height, request->width * 2);
    if (ioctl(state->fd, VIDIOC_S_FMT, &fmt) < 0) {
        (void)hasciicam_virtual_camera_v4l2_describe_error(
            errno, "set format", err, err_size);
        close(state->fd);
        free(state);
        return 0;
    }
    if (fmt.fmt.pix.width != (unsigned int)request->width ||
        fmt.fmt.pix.height != (unsigned int)request->height ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV ||
        fmt.fmt.pix.field != V4L2_FIELD_NONE) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "virtual camera negotiated incompatible format");
        close(state->fd);
        free(state);
        return 0;
    }

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.numerator = 1;
    parm.parm.output.timeperframe.denominator = (unsigned int)request->fps;
    (void)ioctl(state->fd, VIDIOC_S_PARM, &parm);

    state->width = (int)fmt.fmt.pix.width;
    state->height = (int)fmt.fmt.pix.height;
    state->stride_bytes = (int)fmt.fmt.pix.bytesperline;
    state->payload_size = (size_t)fmt.fmt.pix.sizeimage;
    state->payload_buffer = (unsigned char *)calloc(1, state->payload_size);
    if (state->payload_buffer == NULL) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "unable to allocate V4L2 frame buffer");
        close(state->fd);
        free(state);
        return 0;
    }

    *out = hasciicam_virtual_camera_device_create(
        &v4l2_ops, 1, hasciicam_virtual_camera_v4l2_name(), state, err, err_size);
    if (*out == NULL) {
        free(state->payload_buffer);
        close(state->fd);
        free(state);
        return 0;
    }
    return 1;
}
