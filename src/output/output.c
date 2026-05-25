#include "output.h"

#include <string.h>

int hasciicam_output_open_aalib(hasciicam_output *output, aa_context *context) {
    if (output == NULL || context == NULL)
        return 0;
    memset(output, 0, sizeof(*output));
    output->context = context;
    output->adapter_name = "aalib";
    return 1;
}

void hasciicam_output_write_ascii_frame(hasciicam_output *output, int ascii_width, int ascii_height) {
    if (output == NULL || output->context == NULL)
        return;
    aa_fastrender(output->context, 0, 0, ascii_width / 2, ascii_height / 2);
    aa_flush(output->context);
}

void hasciicam_output_poll(hasciicam_output *output) {
    if (output == NULL || output->context == NULL)
        return;
    aa_flush(output->context);
}

void hasciicam_output_close(hasciicam_output *output) {
    if (output == NULL)
        return;
    output->context = NULL;
    output->adapter_name = NULL;
}

const char *hasciicam_output_name(const hasciicam_output *output) {
    if (output == NULL || output->adapter_name == NULL)
        return "unknown";
    return output->adapter_name;
}
