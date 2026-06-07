#include "hasciicam_virtual_camera_pipe.h"

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
