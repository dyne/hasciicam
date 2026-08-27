#include <hasciicam/hasciicam.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned char frame[64 * 32];
    const char *text = NULL;
    int width = 0;
    int height = 0;
    int i;
    int non_space = 0;
    hasciicam_instance *instance = hasciicam_create();
    if (instance == NULL) {
        fprintf(stderr, "create failed\n");
        return 1;
    }

    if (hasciicam_start_external(instance, 0, 32, 32, 16) ||
        hasciicam_start_external_live(instance, 64, 32, 32, 16, NULL) ||
        hasciicam_start_external_live(instance, 64, 32, 32, 16, "missing-driver")) {
        fprintf(stderr, "invalid external start unexpectedly succeeded\n");
        hasciicam_destroy(instance);
        return 2;
    }

    if (!hasciicam_start_external(instance, 64, 32, 32, 16)) {
        fprintf(stderr, "zero-frame start failed\n");
        hasciicam_destroy(instance);
        return 2;
    }
    hasciicam_stop(instance);

    for (i = 0; i < (int)sizeof(frame); i++) {
        frame[i] = (unsigned char)((i * 13) & 0xFF);
    }

    if (!hasciicam_start_external(instance, 64, 32, 32, 16)) {
        fprintf(stderr, "start failed\n");
        hasciicam_destroy(instance);
        return 2;
    }
    if (!hasciicam_submit_frame(instance, frame, sizeof(frame), 64, 32, 64, HASCIICAM_PIXFMT_GRAY8)) {
        fprintf(stderr, "submit failed\n");
        hasciicam_destroy(instance);
        return 3;
    }
    if (!hasciicam_render_frame(instance)) {
        fprintf(stderr, "render failed\n");
        hasciicam_destroy(instance);
        return 4;
    }
    if (!hasciicam_submit_frame(instance, frame, sizeof(frame), 64, 32, 64, HASCIICAM_PIXFMT_GRAY8)) {
        fprintf(stderr, "second submit failed\n");
        hasciicam_destroy(instance);
        return 4;
    }
    if (!hasciicam_render_frame(instance)) {
        fprintf(stderr, "repeated render failed\n");
        hasciicam_destroy(instance);
        return 4;
    }
    if (!hasciicam_get_ascii_frame(instance, &text, NULL, &width, &height)) {
        fprintf(stderr, "frame view failed\n");
        hasciicam_destroy(instance);
        return 5;
    }
    if (text == NULL || width <= 0 || height <= 0) {
        fprintf(stderr, "invalid frame values\n");
        hasciicam_destroy(instance);
        return 6;
    }
    for (i = 0; i < width * height; i++) {
        if (text[i] != ' ' && text[i] != '\0') {
            non_space = 1;
            break;
        }
    }
    if (!non_space) {
        fprintf(stderr, "ascii frame looks empty\n");
        hasciicam_destroy(instance);
        return 7;
    }

    hasciicam_destroy(instance);
    return 0;
}
