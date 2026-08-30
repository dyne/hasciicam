#include <stdio.h>
#include <string.h>

#include "app_session.h"

#ifndef HASCIICAM_SOURCE_DIR
#define HASCIICAM_SOURCE_DIR "."
#endif

static int step_session(hasciicam_session *session) {
    const unsigned char *gray = NULL;
    int gray_size = 0;
    return hasciicam_session_step(session, 8, 6, &gray, &gray_size) && gray != NULL && gray_size == 48;
}

int main(void) {
    hasciicam_session session;
    capture_request camera = {0};
    capture_request image = {0};
    capture_request missing = {0};

    memset(&session, 0, sizeof(session));
    camera.device = "synthetic://";
    image.image_path = HASCIICAM_SOURCE_DIR "/tests/fixtures/image-red.png";
    missing.image_path = HASCIICAM_SOURCE_DIR "/tests/fixtures/missing.png";

    if (!hasciicam_session_start(&session, &image) || strcmp(session.capture_ops->name(), "image") != 0 ||
        !step_session(&session))
        return 1;
    hasciicam_session_set_mirror(&session, 1, 1);
    if (!hasciicam_session_replace(&session, &camera) || strcmp(session.capture_ops->name(), "synthetic") != 0 ||
        !session.mirror_x || !session.mirror_y || !step_session(&session))
        return 2;
    if (!hasciicam_session_replace(&session, &image) || strcmp(session.capture_ops->name(), "image") != 0 ||
        !session.mirror_x || !session.mirror_y || !step_session(&session))
        return 3;
    if (!hasciicam_session_replace(&session, &camera) || strcmp(session.capture_ops->name(), "synthetic") != 0 ||
        !step_session(&session))
        return 4;
    if (hasciicam_session_replace(&session, &missing) || strcmp(session.capture_ops->name(), "synthetic") != 0 ||
        !step_session(&session))
        return 5;
    hasciicam_session_stop(&session);
    return 0;
}
