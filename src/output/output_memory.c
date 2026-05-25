#include "output_memory.h"

#include <stdlib.h>
#include <string.h>

void hasciicam_output_memory_init(hasciicam_memory_output *output) {
    if (output == NULL)
        return;
    memset(output, 0, sizeof(*output));
}

int hasciicam_output_memory_write(hasciicam_memory_output *output,
                                  const hasciicam_ascii_frame *frame) {
    int size;
    if (output == NULL || frame == NULL || frame->text == NULL || frame->attrs == NULL)
        return 0;
    if (frame->width <= 0 || frame->height <= 0)
        return 0;

    size = frame->width * frame->height;
    if (size <= 0)
        return 0;

    if (output->capacity < size) {
        char *new_text = (char *)malloc((size_t)size);
        char *new_attrs = (char *)malloc((size_t)size);
        if (new_text == NULL || new_attrs == NULL) {
            free(new_text);
            free(new_attrs);
            return 0;
        }
        free(output->text);
        free(output->attrs);
        output->text = new_text;
        output->attrs = new_attrs;
        output->capacity = size;
    }

    memcpy(output->text, frame->text, (size_t)size);
    memcpy(output->attrs, frame->attrs, (size_t)size);
    output->width = frame->width;
    output->height = frame->height;
    return 1;
}

void hasciicam_output_memory_close(hasciicam_memory_output *output) {
    if (output == NULL)
        return;
    free(output->text);
    free(output->attrs);
    memset(output, 0, sizeof(*output));
}
