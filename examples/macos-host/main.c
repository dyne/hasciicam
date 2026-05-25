#include <hasciicam/hasciicam.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned char frame[64 * 32];
    const char *text = NULL;
    int x;
    int y;
    int w = 0;
    int h = 0;
    hasciicam_instance *instance = hasciicam_create();
    if (instance == NULL) {
        fprintf(stderr, "failed to create hasciicam instance\n");
        return 1;
    }

    for (y = 0; y < 32; y++) {
        for (x = 0; x < 64; x++) {
            frame[y * 64 + x] = (unsigned char)((x * 255) / 63);
        }
    }

    if (!hasciicam_start_external(instance, 64, 32, 32, 16)) {
        fprintf(stderr, "failed to start external session\n");
        hasciicam_destroy(instance);
        return 2;
    }

    if (!hasciicam_submit_frame(instance, frame, sizeof(frame), 64, 32, 64, HASCIICAM_PIXFMT_GRAY8)) {
        fprintf(stderr, "failed to submit frame\n");
        hasciicam_destroy(instance);
        return 3;
    }
    if (!hasciicam_render_frame(instance)) {
        fprintf(stderr, "failed to render frame\n");
        hasciicam_destroy(instance);
        return 4;
    }
    if (!hasciicam_get_ascii_frame(instance, &text, NULL, &w, &h) || text == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "failed to get ascii frame\n");
        hasciicam_destroy(instance);
        return 5;
    }

    fwrite(text, 1, (size_t)(w * h), stdout);
    fputc('\n', stdout);

    hasciicam_destroy(instance);
    return 0;
}
