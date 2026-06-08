#include "virtual_camera.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include "windows/pipe/hasciicam_virtual_camera_pipe.h"
#elif defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "virtual_camera_v4l2.h"
#endif

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
#if defined(_WIN32)
    HANDLE pipe_handle;
    HANDLE accept_thread;
    HANDLE stop_event;
    LONG connected;
    unsigned long long sequence;
    hasciicam_virtual_camera_request request;
    char pipe_name[256];
    unsigned char *message_buffer;
    size_t message_size;
    size_t payload_size;
#elif defined(__linux__)
    int fd;
    int requested_width;
    int requested_height;
    int requested_fps;
    int width;
    int height;
    int stride_bytes;
    size_t payload_size;
    unsigned char *payload_buffer;
    int disconnected;
#endif
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

#if defined(__linux__)
static int linux_v4l2_set_error(char *err, size_t err_size, const char *msg) {
    set_error(err, err_size, msg);
    return 0;
}

int hasciicam_virtual_camera_v4l2_is_output_capable(unsigned int capabilities) {
    return (capabilities & V4L2_CAP_VIDEO_OUTPUT) != 0 ||
           (capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) != 0;
}

int hasciicam_virtual_camera_v4l2_should_retry_write(int errnum) {
    return errnum == EINTR;
}

int hasciicam_virtual_camera_v4l2_should_drop_frame(int errnum) {
    return errnum == EAGAIN || errnum == EWOULDBLOCK;
}

int hasciicam_virtual_camera_v4l2_should_disconnect_write(ssize_t written, size_t payload_size, int errnum) {
    if (written == (ssize_t)payload_size)
        return 0;
    if (written < 0 && hasciicam_virtual_camera_v4l2_should_retry_write(errnum))
        return 0;
    if (written < 0 && hasciicam_virtual_camera_v4l2_should_drop_frame(errnum))
        return 0;
    return 1;
}

static int linux_v4l2_open_device(const hasciicam_virtual_camera_request *request,
                                  int *fd_out,
                                  int *width_out,
                                  int *height_out,
                                  int *stride_out,
                                  size_t *payload_out,
                                  char *err,
                                  size_t err_size) {
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;

    if (request == NULL || fd_out == NULL || width_out == NULL || height_out == NULL ||
        stride_out == NULL || payload_out == NULL) {
        return linux_v4l2_set_error(err, err_size, "virtual camera request is required");
    }

    fd = open(request->device, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        return linux_v4l2_set_error(err, err_size, "unable to open virtual camera output device");
    }

    memset(&cap, 0, sizeof(cap));
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        return linux_v4l2_set_error(err, err_size, "virtual camera device does not support V4L2 querycap");
    }
    if (!hasciicam_virtual_camera_v4l2_is_output_capable(cap.capabilities)) {
        close(fd);
        return linux_v4l2_set_error(err, err_size, "virtual camera device is not a V4L2 output node");
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = (unsigned int)request->width;
    fmt.fmt.pix.height = (unsigned int)request->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = (unsigned int)(request->width * 2);
    fmt.fmt.pix.sizeimage = (unsigned int)hasciicam_virtual_camera_yuy2_size(request->width,
                                                                             request->height,
                                                                             request->width * 2);
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        close(fd);
        return linux_v4l2_set_error(err, err_size, "virtual camera rejected YUYV format");
    }
    if (fmt.fmt.pix.width != (unsigned int)request->width ||
        fmt.fmt.pix.height != (unsigned int)request->height ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV ||
        fmt.fmt.pix.field != V4L2_FIELD_NONE) {
        close(fd);
        return linux_v4l2_set_error(err, err_size, "virtual camera negotiated incompatible format");
    }

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.numerator = 1;
    parm.parm.output.timeperframe.denominator = (request->fps > 0) ? (unsigned int)request->fps : 30U;
    (void)ioctl(fd, VIDIOC_S_PARM, &parm);

    *fd_out = fd;
    *width_out = (int)fmt.fmt.pix.width;
    *height_out = (int)fmt.fmt.pix.height;
    *stride_out = (int)fmt.fmt.pix.bytesperline;
    *payload_out = (size_t)fmt.fmt.pix.sizeimage;
    return 1;
}

static int linux_v4l2_publish(hasciicam_virtual_camera_device *device,
                              const hasciicam_virtual_camera_frame *frame) {
    ssize_t written;

    if (device == NULL || frame == NULL || device->fd < 0 || device->payload_buffer == NULL)
        return 0;
    if (device->disconnected)
        return 1;
    if (frame->pixel_format != HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32 || frame->pixels == NULL)
        return 0;
    if (!hasciicam_virtual_camera_scale_bgra32_to_yuy2(frame->pixels,
                                                       frame->width,
                                                       frame->height,
                                                       frame->stride_bytes,
                                                       device->payload_buffer,
                                                       device->width,
                                                       device->height,
                                                       device->stride_bytes,
                                                       0,
                                                       0)) {
        return 0;
    }
    for (;;) {
        written = write(device->fd, device->payload_buffer, device->payload_size);
        if (written == (ssize_t)device->payload_size)
            return 1;
        if (written < 0 && hasciicam_virtual_camera_v4l2_should_retry_write(errno))
            continue;
        if (written < 0 && hasciicam_virtual_camera_v4l2_should_drop_frame(errno))
            return 1;
        if (!hasciicam_virtual_camera_v4l2_should_disconnect_write(written, device->payload_size, errno))
            return 1;
        device->disconnected = 1;
        close(device->fd);
        device->fd = -1;
        return 1;
    }
}

static void linux_v4l2_close(hasciicam_virtual_camera_device *device) {
    if (device == NULL)
        return;
    if (device->fd >= 0)
        close(device->fd);
    free(device->payload_buffer);
    free(device);
}

static const char *linux_v4l2_name(void) {
    return "v4l2-output";
}

static const hasciicam_virtual_camera_ops linux_v4l2_ops = {
    linux_v4l2_publish,
    linux_v4l2_close,
    linux_v4l2_name
};
#endif

#if defined(_WIN32)
static DWORD WINAPI windows_accept_thread(LPVOID param) {
    hasciicam_virtual_camera_device *device = (hasciicam_virtual_camera_device *)param;

    if (device == NULL || device->pipe_handle == INVALID_HANDLE_VALUE)
        return 0;
    while (WaitForSingleObject(device->stop_event, 0) != WAIT_OBJECT_0) {
        BOOL connected = ConnectNamedPipe(device->pipe_handle, NULL);
        if (!connected) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED && err != ERROR_NO_DATA) {
                Sleep(50);
                continue;
            }
        }
        InterlockedExchange(&device->connected, 1);
        while (WaitForSingleObject(device->stop_event, 0) != WAIT_OBJECT_0) {
            if (WaitForSingleObject(device->stop_event, 25) == WAIT_OBJECT_0)
                break;
            if (InterlockedCompareExchange(&device->connected, 1, 1) != 1)
                break;
        }
        DisconnectNamedPipe(device->pipe_handle);
        InterlockedExchange(&device->connected, 0);
    }
    return 0;
}

static int windows_virtual_camera_publish(hasciicam_virtual_camera_device *device,
                                          const hasciicam_virtual_camera_frame *frame) {
    hasciicam_virtual_camera_pipe_frame header;
    DWORD bytes_written = 0;

    if (device == NULL || frame == NULL)
        return 0;
    if (InterlockedCompareExchange(&device->connected, 1, 1) != 1)
        return 1;
    if (device->message_buffer == NULL || device->message_size == 0)
        return 0;
    if (frame->pixel_format != HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32 || frame->pixels == NULL)
        return 0;

    hasciicam_virtual_camera_pipe_frame_init(&header,
                                             HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                             device->request.width,
                                             device->request.height,
                                             device->request.width * 2,
                                             device->sequence++,
                                             frame->timestamp_100ns);
    if (!hasciicam_virtual_camera_scale_bgra32_to_yuy2(frame->pixels,
                                                       frame->width,
                                                       frame->height,
                                                       frame->stride_bytes,
                                                       device->message_buffer + sizeof(header),
                                                       device->request.width,
                                                       device->request.height,
                                                       device->request.width * 2,
                                                       0,
                                                       0)) {
        return 0;
    }
    if (!hasciicam_virtual_camera_pipe_encode_message(&header,
                                                      device->message_buffer + sizeof(header),
                                                      device->payload_size,
                                                      device->message_buffer,
                                                      device->message_size,
                                                      NULL,
                                                      0)) {
        return 0;
    }
    if (!WriteFile(device->pipe_handle,
                   device->message_buffer,
                   (DWORD)device->message_size,
                   &bytes_written,
                   NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
            InterlockedExchange(&device->connected, 0);
    }
    return 1;
}

static void windows_virtual_camera_close(hasciicam_virtual_camera_device *device) {
    if (device == NULL)
        return;
    if (device->stop_event != NULL)
        SetEvent(device->stop_event);
    if (device->accept_thread != NULL) {
        WaitForSingleObject(device->accept_thread, INFINITE);
        CloseHandle(device->accept_thread);
        device->accept_thread = NULL;
    }
    if (device->pipe_handle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(device->pipe_handle);
        CloseHandle(device->pipe_handle);
        device->pipe_handle = INVALID_HANDLE_VALUE;
    }
    if (device->stop_event != NULL) {
        CloseHandle(device->stop_event);
        device->stop_event = NULL;
    }
    free(device->message_buffer);
    free(device);
}

static const char *windows_virtual_camera_name(void) {
    return "windows-pipe";
}

static const hasciicam_virtual_camera_ops windows_virtual_camera_ops = {
    windows_virtual_camera_publish,
    windows_virtual_camera_close,
    windows_virtual_camera_name
};
#endif

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

#if defined(_WIN32)
    if (request == NULL || !request->enabled) {
        device->ops = &unsupported_ops;
        device->supported = 0;
        strncpy(device->backend_name, unsupported_name(), sizeof(device->backend_name) - 1);
        *out = device;
        return 1;
    }
    device->ops = &windows_virtual_camera_ops;
    device->supported = 1;
    strncpy(device->backend_name, windows_virtual_camera_name(), sizeof(device->backend_name) - 1);
    device->request = *request;
    if (!hasciicam_virtual_camera_pipe_build_name(request,
                                                  device->pipe_name,
                                                  sizeof(device->pipe_name),
                                                  err,
                                                  sizeof(err))) {
        free(device);
        return 0;
    }
    device->payload_size = hasciicam_virtual_camera_yuy2_size(request->width, request->height, request->width * 2);
    device->message_size = sizeof(hasciicam_virtual_camera_pipe_frame) + device->payload_size;
    device->message_buffer = (unsigned char *)calloc(1, device->message_size);
    if (device->message_buffer == NULL) {
        free(device);
        return 0;
    }
    device->pipe_handle = CreateNamedPipeA(device->pipe_name,
                                           PIPE_ACCESS_OUTBOUND,
                                           PIPE_TYPE_BYTE | PIPE_WAIT,
                                           1,
                                           (DWORD)device->message_size,
                                           (DWORD)device->message_size,
                                           0,
                                           NULL);
    if (device->pipe_handle == INVALID_HANDLE_VALUE) {
        free(device->message_buffer);
        free(device);
        return 0;
    }
    device->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (device->stop_event == NULL) {
        CloseHandle(device->pipe_handle);
        free(device->message_buffer);
        free(device);
        return 0;
    }
    device->accept_thread = CreateThread(NULL, 0, windows_accept_thread, device, 0, NULL);
    if (device->accept_thread == NULL) {
        CloseHandle(device->stop_event);
        CloseHandle(device->pipe_handle);
        free(device->message_buffer);
        free(device);
        return 0;
    }
#elif defined(__linux__)
    if (request == NULL || !request->enabled) {
        device->ops = &unsupported_ops;
        device->supported = 0;
        strncpy(device->backend_name, unsupported_name(), sizeof(device->backend_name) - 1);
        *out = device;
        return 1;
    }
    if (request->device[0] == '\0') {
        free(device);
        return 0;
    }
    device->ops = &linux_v4l2_ops;
    device->supported = 1;
    strncpy(device->backend_name, linux_v4l2_name(), sizeof(device->backend_name) - 1);
    device->requested_width = request->width;
    device->requested_height = request->height;
    device->requested_fps = request->fps;
    if (!linux_v4l2_open_device(request,
                                &device->fd,
                                &device->width,
                                &device->height,
                                &device->stride_bytes,
                                &device->payload_size,
                                err,
                                sizeof(err))) {
        free(device);
        return 0;
    }
    device->payload_buffer = (unsigned char *)calloc(1, device->payload_size);
    if (device->payload_buffer == NULL) {
        close(device->fd);
        free(device);
        return 0;
    }
#else
    device->ops = &unsupported_ops;
    device->supported = 0;
    strncpy(device->backend_name, unsupported_name(), sizeof(device->backend_name) - 1);
#endif
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
