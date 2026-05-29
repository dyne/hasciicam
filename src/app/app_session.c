#include "app_session.h"

#include <stdlib.h>
#include <string.h>

#include "../capture/capture_backend.h"
#include "../capture/capture_external.h"
#include "../capture/frame_convert.h"

int hasciicam_session_start(hasciicam_session *session, const capture_request *req) {
    if (session == NULL || req == NULL)
        return 0;
    memset(session, 0, sizeof(*session));

    if (!capture_open_default(req, &session->capture_dev, &session->capture_ops))
        return 0;
    if (!session->capture_ops->describe(session->capture_dev, &session->capture_info)) {
        session->capture_ops->close(session->capture_dev);
        memset(session, 0, sizeof(*session));
        return 0;
    }
    if (!session->capture_ops->start(session->capture_dev)) {
        session->capture_ops->close(session->capture_dev);
        memset(session, 0, sizeof(*session));
        return 0;
    }
    return 1;
}

void hasciicam_session_set_mirror(hasciicam_session *session, int mirror_x, int mirror_y) {
    if (session == NULL)
        return;
    session->mirror_x = mirror_x ? 1 : 0;
    session->mirror_y = mirror_y ? 1 : 0;
}

int hasciicam_session_step(hasciicam_session *session,
                           int output_width,
                           int output_height,
                           const unsigned char **gray_frame,
                           int *gray_size) {
    capture_frame frame;
    int required_size;

    if (session == NULL || session->capture_dev == NULL || session->capture_ops == NULL)
        return 0;
    if (session->stop_requested)
        return 0;
    if (output_width <= 0 || output_height <= 0)
        return 0;

    required_size = output_width * output_height;
    if (required_size <= 0)
        return 0;

    if (session->gray_size != required_size) {
        unsigned char *new_buffer = (unsigned char *)malloc((size_t)required_size);
        if (new_buffer == NULL)
            return 0;
        free(session->gray_buffer);
        session->gray_buffer = new_buffer;
        session->gray_size = required_size;
    }

    memset(&frame, 0, sizeof(frame));
    if (!session->capture_ops->read(session->capture_dev, &frame))
        return 0;

    if (!capture_frame_to_gray_scaled_mirrored(&frame,
                                               session->gray_buffer,
                                               output_width,
                                               output_height,
                                               session->mirror_x,
                                               session->mirror_y)) {
        session->capture_ops->release(session->capture_dev, &frame);
        return 0;
    }
    session->capture_ops->release(session->capture_dev, &frame);

    if (gray_frame != NULL)
        *gray_frame = session->gray_buffer;
    if (gray_size != NULL)
        *gray_size = session->gray_size;
    return 1;
}

int hasciicam_session_submit_frame(const unsigned char *data,
                                   size_t data_size,
                                   int width,
                                   int height,
                                   int stride_bytes,
                                   capture_pixel_format pixel_format) {
    return capture_external_submit_frame(data, data_size, width, height, stride_bytes, pixel_format);
}

void hasciicam_session_stop(hasciicam_session *session) {
    if (session == NULL)
        return;
    if (session->capture_ops != NULL && session->capture_dev != NULL) {
        session->capture_ops->stop(session->capture_dev);
        session->capture_ops->close(session->capture_dev);
    }
    free(session->gray_buffer);
    memset(session, 0, sizeof(*session));
}

const capture_info *hasciicam_session_capture_info(const hasciicam_session *session) {
    if (session == NULL)
        return NULL;
    return &session->capture_info;
}

void hasciicam_session_request_stop(hasciicam_session *session) {
    if (session == NULL)
        return;
    session->stop_requested = 1;
}

int hasciicam_session_should_stop(const hasciicam_session *session) {
    if (session == NULL)
        return 1;
    return session->stop_requested;
}
