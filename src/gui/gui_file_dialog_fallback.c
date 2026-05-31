#include "gui_file_dialog.h"

#include <stdio.h>

hasciicam_gui_file_dialog_result hasciicam_gui_select_toml_file(char *out_path,
                                                                size_t out_path_size,
                                                                char *err,
                                                                size_t err_size) {
    (void)out_path;
    (void)out_path_size;
    if (err != NULL && err_size > 0)
        snprintf(err, err_size, "native file dialog is not available on this platform");
    return HASCIICAM_GUI_FILE_DIALOG_NOT_AVAILABLE;
}
