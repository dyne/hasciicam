#ifndef HASCIICAM_APP_SESSION_H
#define HASCIICAM_APP_SESSION_H

#include "../capture/capture.h"

typedef struct hasciicam_session {
    capture_device *capture_dev;
    const capture_ops *capture_ops;
    capture_info capture_info;
    unsigned char *gray_buffer;
    int gray_size;
} hasciicam_session;

int hasciicam_session_start(hasciicam_session *session, const capture_request *req);
int hasciicam_session_step(hasciicam_session *session,
                           int output_width,
                           int output_height,
                           const unsigned char **gray_frame,
                           int *gray_size);
void hasciicam_session_stop(hasciicam_session *session);
const capture_info *hasciicam_session_capture_info(const hasciicam_session *session);

#endif
