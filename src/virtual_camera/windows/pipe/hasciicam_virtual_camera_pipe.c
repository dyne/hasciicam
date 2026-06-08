#include "hasciicam_virtual_camera_pipe.h"

#if defined(_WIN32)

#include <windows.h>
#include <lmcons.h>
#include <sddl.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
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

static int is_supported_dimension(int width, int height) {
    return width > 0 && height > 0 &&
           width <= HASCIICAM_VIRTUAL_CAMERA_PIPE_MAX_WIDTH &&
           height <= HASCIICAM_VIRTUAL_CAMERA_PIPE_MAX_HEIGHT;
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

static int current_user_sid_string(char *out, size_t out_size, char *err, size_t err_size) {
#if defined(_WIN32)
    HANDLE token = NULL;
    DWORD needed = 0;
    TOKEN_USER *user = NULL;
    char *sid_string = NULL;
    int ok = 0;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "SID buffer is required");
        return 0;
    }
    out[0] = '\0';
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        set_error(err, err_size, "unable to open process token");
        return 0;
    }
    if (!GetTokenInformation(token, TokenUser, NULL, 0, &needed) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        set_error(err, err_size, "unable to query token size");
        goto cleanup;
    }
    user = (TOKEN_USER *)malloc(needed);
    if (user == NULL) {
        set_error(err, err_size, "unable to allocate token buffer");
        goto cleanup;
    }
    if (!GetTokenInformation(token, TokenUser, user, needed, &needed)) {
        set_error(err, err_size, "unable to query token user");
        goto cleanup;
    }
    if (!ConvertSidToStringSidA(user->User.Sid, &sid_string)) {
        set_error(err, err_size, "unable to convert SID");
        goto cleanup;
    }
    if (snprintf(out, out_size, "%s", sid_string) < 0 || out[0] == '\0') {
        set_error(err, err_size, "SID buffer too small");
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (sid_string != NULL)
        LocalFree(sid_string);
    free(user);
    if (token != NULL)
        CloseHandle(token);
    return ok;
#else
    (void)out;
    (void)out_size;
    set_error(err, err_size, "current user SID is only available on Windows");
    return 0;
#endif
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

size_t hasciicam_virtual_camera_pipe_frame_message_size(const hasciicam_virtual_camera_pipe_frame *frame) {
    size_t payload_size;

    payload_size = hasciicam_virtual_camera_pipe_frame_payload_size(frame);
    if (payload_size == 0)
        return 0;
    if (payload_size > SIZE_MAX - sizeof(*frame))
        return 0;
    return sizeof(*frame) + payload_size;
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
    if (!is_supported_dimension(frame->width, frame->height)) {
        set_error(err, err_size, "pipe frame size must be within supported bounds");
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

int hasciicam_virtual_camera_pipe_encode_message(const hasciicam_virtual_camera_pipe_frame *frame,
                                                 const void *payload,
                                                 size_t payload_size,
                                                 void *bytes,
                                                 size_t bytes_size,
                                                 char *err,
                                                 size_t err_size) {
    size_t message_size;

    if (frame == NULL || payload == NULL || bytes == NULL) {
        set_error(err, err_size, "pipe message buffers are required");
        return 0;
    }
    if (!hasciicam_virtual_camera_pipe_frame_validate(frame, err, err_size))
        return 0;
    message_size = hasciicam_virtual_camera_pipe_frame_message_size(frame);
    if (message_size == 0 || message_size != sizeof(*frame) + payload_size) {
        set_error(err, err_size, "pipe payload size mismatch");
        return 0;
    }
    if (bytes_size < message_size) {
        set_error(err, err_size, "pipe message buffer too small");
        return 0;
    }
    memcpy(bytes, frame, sizeof(*frame));
    memcpy((unsigned char *)bytes + sizeof(*frame), payload, payload_size);
    return 1;
}

int hasciicam_virtual_camera_pipe_build_name(const hasciicam_virtual_camera_request *request,
                                             char *out,
                                             size_t out_size,
                                             char *err,
                                             size_t err_size) {
    char device[128];
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

    cursor = out;
    remaining = out_size;
    if (!append_formatted(&cursor,
                          &remaining,
                          "\\\\.\\pipe\\HasciiCam_%s_%dx%d@%d",
                          device[0] != '\0' ? device : "default",
                          request->width,
                          request->height,
                          request->fps))
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

int hasciicam_virtual_camera_pipe_build_sddl(char *out,
                                             size_t out_size,
                                             char *err,
                                             size_t err_size) {
    char sid[128];
    char *cursor;
    size_t remaining;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "security descriptor buffer is required");
        return 0;
    }
    out[0] = '\0';
    clear_error(err, err_size);
    if (!current_user_sid_string(sid, sizeof(sid), err, err_size))
        return 0;

    cursor = out;
    remaining = out_size;
    if (!append_text(&cursor, &remaining, "D:P("))
        goto fail;
    if (!append_text(&cursor, &remaining, "A;;GA;;;SY)"))
        goto fail;
    if (!append_text(&cursor, &remaining, "(A;;GA;;;LS)"))
        goto fail;
    if (!append_formatted(&cursor, &remaining, "(A;;GA;;;%s)", sid))
        goto fail;
    return 1;

fail:
    set_error(err, err_size, "security descriptor buffer too small");
    return 0;
}

int hasciicam_virtual_camera_pipe_decode_message(const void *bytes,
                                                 size_t bytes_size,
                                                 hasciicam_virtual_camera_pipe_frame *header_out,
                                                 const unsigned char **payload_out,
                                                 size_t *payload_size_out,
                                                 char *err,
                                                 size_t err_size) {
    const hasciicam_virtual_camera_pipe_frame *frame;
    size_t payload_size;
    size_t expected_size;

    if (bytes == NULL) {
        set_error(err, err_size, "pipe message is required");
        return 0;
    }
    if (bytes_size < sizeof(hasciicam_virtual_camera_pipe_frame)) {
        set_error(err, err_size, "pipe message is truncated");
        return 0;
    }

    frame = (const hasciicam_virtual_camera_pipe_frame *)bytes;
    if (!hasciicam_virtual_camera_pipe_frame_validate(frame, err, err_size))
        return 0;

    payload_size = (size_t)frame->payload_bytes;
    expected_size = sizeof(*frame) + payload_size;
    if (bytes_size != expected_size) {
        set_error(err, err_size, bytes_size < expected_size ? "pipe message payload is truncated"
                                                            : "pipe message has trailing bytes");
        return 0;
    }

    if (header_out != NULL)
        *header_out = *frame;
    if (payload_out != NULL)
        *payload_out = (const unsigned char *)bytes + sizeof(*frame);
    if (payload_size_out != NULL)
        *payload_size_out = payload_size;
    return 1;
}
