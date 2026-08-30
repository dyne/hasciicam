#include "output_text_frame.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
        error[error_size - 1] = '\0';
    }
}

int hasciicam_output_text_frame_write(const hasciicam_ascii_frame *frame,
                                      const char *destination,
                                      char *error,
                                      size_t error_size) {
    char temporary[1024];
    FILE *file = NULL;
    int row;

    if (error != NULL && error_size > 0)
        error[0] = '\0';
    if (frame == NULL || frame->text == NULL || destination == NULL || destination[0] == '\0') {
        set_error(error, error_size, "frame text and destination are required");
        return 0;
    }
    if (frame->width <= 0 || frame->height <= 0 ||
        (size_t)frame->width > SIZE_MAX / (size_t)frame->height ||
        frame->width > INT_MAX / frame->height) {
        set_error(error, error_size, "invalid frame dimensions");
        return 0;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", destination) >= (int)sizeof(temporary)) {
        set_error(error, error_size, "destination path is too long");
        return 0;
    }
    file = fopen(temporary, "wb");
    if (file == NULL) {
        set_error(error, error_size, "cannot open temporary text frame");
        return 0;
    }
    for (row = 0; row < frame->height; ++row) {
        const char *line = frame->text + (size_t)row * (size_t)frame->width;
        if (fwrite(line, 1, (size_t)frame->width, file) != (size_t)frame->width ||
            fputc('\n', file) == EOF) {
            fclose(file);
            remove(temporary);
            set_error(error, error_size, "cannot write text frame");
            return 0;
        }
    }
    if (fclose(file) != 0) {
        remove(temporary);
        set_error(error, error_size, "cannot close text frame");
        return 0;
    }
#if defined(_WIN32)
    remove(destination);
#endif
    if (rename(temporary, destination) != 0) {
        remove(temporary);
        set_error(error, error_size, "cannot replace text frame");
        return 0;
    }
    return 1;
}
