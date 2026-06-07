#include "hasciicam_virtual_camera_pipe.h"

#if defined(_WIN32)

#include <windows.h>
#include <lmcons.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    if (msg == NULL)
        msg = "invalid virtual camera pipe frame";
    strncpy(err, msg, err_size - 1);
    err[err_size - 1] = '\0';
}

static int is_supported_format(hasciicam_virtual_camera_pixel_format pixel_format) {
    return pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2 ||
           pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_NV12;
}

static void clear_error(char *err, size_t err_size) {
    if (err != NULL && err_size > 0)
        err[0] = '\0';
}

static int append_text(char **cursor, size_t *remaining, const char *text) {
    int written;

    if (cursor == NULL || remaining == NULL || text == NULL)
        return 0;
    if (*remaining == 0)
        return 0;
    written = snprintf(*cursor, *remaining, "%s", text);
    if (written < 0 || (size_t)written >= *remaining)
        return 0;
    *cursor += written;
    *remaining -= (size_t)written;
    return 1;
}

static int append_formatted(char **cursor, size_t *remaining, const char *fmt, ...) {
    int written;
    va_list args;

    if (cursor == NULL || remaining == NULL || fmt == NULL)
        return 0;
    if (*remaining == 0)
        return 0;
    va_start(args, fmt);
    written = vsnprintf(*cursor, *remaining, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= *remaining)
        return 0;
    *cursor += written;
    *remaining -= (size_t)written;
    return 1;
}

static int sanitize_component(const char *text, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;

    if (out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if (text == NULL || text[0] == '\0')
        return 1;
    for (i = 0; text[i] != '\0' && j + 1 < out_size; ++i) {
        unsigned char c = (unsigned char)text[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            out[j++] = (char)c;
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    return 1;
}

static const char *current_username(void) {
#if defined(_WIN32)
    static char username[UNLEN + 1];
    DWORD size = sizeof(username);
    if (GetUserNameA(username, &size) && username[0] != '\0')
        return username;
#endif
    return "current-user";
}

void hasciicam_virtual_camera_pipe_frame_init(hasciicam_virtual_camera_pipe_frame *frame,
                                              hasciicam_virtual_camera_pixel_format pixel_format,
                                              int width,
                                              int height,
                                              int stride_bytes,
                                              uint64_t sequence,
                                              uint64_t timestamp_100ns) {
    if (frame == NULL)
        return;
    memset(frame, 0, sizeof(*frame));
    frame->magic = HASCIICAM_VIRTUAL_CAMERA_PIPE_MAGIC;
    frame->version = HASCIICAM_VIRTUAL_CAMERA_PIPE_VERSION;
    frame->header_size = (uint16_t)sizeof(*frame);
    frame->pixel_format = (uint32_t)pixel_format;
    frame->width = width;
    frame->height = height;
    frame->stride_bytes = stride_bytes;
    frame->sequence = sequence;
    frame->timestamp_100ns = timestamp_100ns;
    frame->payload_bytes = (uint32_t)hasciicam_virtual_camera_pipe_frame_payload_size(frame);
}

size_t hasciicam_virtual_camera_pipe_frame_payload_size(const hasciicam_virtual_camera_pipe_frame *frame) {
    if (frame == NULL)
        return 0;
    if (!is_supported_format((hasciicam_virtual_camera_pixel_format)frame->pixel_format))
        return 0;
    if ((hasciicam_virtual_camera_pixel_format)frame->pixel_format == HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2) {
        return hasciicam_virtual_camera_yuy2_size(frame->width, frame->height, frame->stride_bytes);
    }
    return hasciicam_virtual_camera_nv12_size(frame->width,
                                              frame->height,
                                              frame->stride_bytes,
                                              frame->stride_bytes);
}

int hasciicam_virtual_camera_pipe_frame_validate(const hasciicam_virtual_camera_pipe_frame *frame,
                                                 char *err,
                                                 size_t err_size) {
    size_t expected_bytes;

    if (frame == NULL) {
        set_error(err, err_size, "null pipe frame");
        return 0;
    }
    if (frame->magic != HASCIICAM_VIRTUAL_CAMERA_PIPE_MAGIC) {
        set_error(err, err_size, "bad pipe magic");
        return 0;
    }
    if (frame->version != HASCIICAM_VIRTUAL_CAMERA_PIPE_VERSION) {
        set_error(err, err_size, "unsupported pipe version");
        return 0;
    }
    if (frame->header_size != sizeof(*frame)) {
        set_error(err, err_size, "bad pipe header size");
        return 0;
    }
    if (!is_supported_format((hasciicam_virtual_camera_pixel_format)frame->pixel_format)) {
        set_error(err, err_size, "unsupported pipe pixel format");
        return 0;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        set_error(err, err_size, "pipe frame size must be positive");
        return 0;
    }
    if ((frame->width & 1) != 0 || (frame->height & 1) != 0) {
        set_error(err, err_size, "pipe frame size must be even");
        return 0;
    }
    if (frame->stride_bytes <= 0) {
        set_error(err, err_size, "pipe frame stride must be positive");
        return 0;
    }

    expected_bytes = hasciicam_virtual_camera_pipe_frame_payload_size(frame);
    if (expected_bytes == 0 || expected_bytes > UINT32_MAX) {
        set_error(err, err_size, "pipe frame payload is invalid");
        return 0;
    }
    if (frame->payload_bytes != (uint32_t)expected_bytes) {
        set_error(err, err_size, "pipe frame payload mismatch");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_pipe_build_name(const hasciicam_virtual_camera_request *request,
                                             char *out,
                                             size_t out_size,
                                             char *err,
                                             size_t err_size) {
    char device[128];
    char user[64];
    char *cursor;
    size_t remaining;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "pipe name buffer is required");
        return 0;
    }
    out[0] = '\0';
    clear_error(err, err_size);
    if (request == NULL || !request->enabled) {
        set_error(err, err_size, "virtual camera request must be enabled");
        return 0;
    }
    if (!sanitize_component(request->device, device, sizeof(device)))
        return 0;
    if (!sanitize_component(current_username(), user, sizeof(user)))
        return 0;

    cursor = out;
    remaining = out_size;
    if (!append_text(&cursor, &remaining, "\\\\.\\pipe\\HasciiCam\\"))
        goto fail;
    if (!append_text(&cursor, &remaining, user))
        goto fail;
    if (!append_text(&cursor, &remaining, "\\"))
        goto fail;
    if (!append_text(&cursor, &remaining, device[0] != '\0' ? device : "default"))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "\\%dx%d@%d", request->width, request->height, request->fps))
        goto fail;
    return 1;

fail:
    set_error(err, err_size, "pipe name buffer too small");
    return 0;
}

int hasciicam_virtual_camera_pipe_build_registration_payload(const hasciicam_virtual_camera_request *request,
                                                             char *out,
                                                             size_t out_size,
                                                             char *err,
                                                             size_t err_size) {
    char pipe_name[256];
    char device[128];
    char *cursor;
    size_t remaining;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "registration payload buffer is required");
        return 0;
    }
    out[0] = '\0';
    clear_error(err, err_size);
    if (request == NULL || !request->enabled) {
        set_error(err, err_size, "virtual camera request must be enabled");
        return 0;
    }
    if (!sanitize_component(request->device, device, sizeof(device)))
        return 0;
    if (!hasciicam_virtual_camera_pipe_build_name(request, pipe_name, sizeof(pipe_name), err, err_size))
        return 0;

    cursor = out;
    remaining = out_size;
    if (!append_formatted(&cursor, &remaining, "v=%u;", HASCIICAM_VIRTUAL_CAMERA_PIPE_VERSION))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "pipe=%s;", pipe_name))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "user=%s;", current_username()))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "device=%s;", device[0] != '\0' ? device : "default"))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "size=%dx%d;", request->width, request->height))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "fps=%d;", request->fps))
        goto fail;
    if (!append_text(&cursor, &remaining, "fmt=yuy2;"))
        goto fail;
    return 1;

fail:
    set_error(err, err_size, "registration payload buffer too small");
    return 0;
}
