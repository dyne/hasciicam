#ifndef HASCIICAM_APP_SESSION_H
#define HASCIICAM_APP_SESSION_H

#include "../capture/capture.h"

typedef struct hasciicam_session {
    capture_device *capture_dev;
    const capture_ops *capture_ops;
    capture_info capture_info;
    unsigned char *gray_buffer;
    int gray_size;
    int mirror_x;
    int mirror_y;
    int stop_requested;
} hasciicam_session;

int hasciicam_session_start(hasciicam_session *session, const capture_request *req);
/* Replace the active capture only after a fully started candidate is ready. */
int hasciicam_session_replace(hasciicam_session *session, const capture_request *req);
void hasciicam_session_set_mirror(hasciicam_session *session, int mirror_x, int mirror_y);
int hasciicam_session_step(hasciicam_session *session,
                           int output_width,
                           int output_height,
                           const unsigned char **gray_frame,
                           int *gray_size);
int hasciicam_session_submit_frame(const unsigned char *data,
                                   size_t data_size,
                                   int width,
                                   int height,
                                   int stride_bytes,
                                   capture_pixel_format pixel_format);
void hasciicam_session_stop(hasciicam_session *session);
const capture_info *hasciicam_session_capture_info(const hasciicam_session *session);
void hasciicam_session_request_stop(hasciicam_session *session);
int hasciicam_session_should_stop(const hasciicam_session *session);
int hasciicam_session_list_controls(hasciicam_session *session,
                                    capture_control_desc *out,
                                    int max_controls);
int hasciicam_session_set_control(hasciicam_session *session,
                                  capture_control_id id,
                                  int value);
int hasciicam_session_set_control_auto(hasciicam_session *session,
                                       capture_control_id id,
                                       int enabled);

#endif
