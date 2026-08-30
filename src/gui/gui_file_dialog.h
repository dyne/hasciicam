#ifndef HASCIICAM_GUI_FILE_DIALOG_H
#define HASCIICAM_GUI_FILE_DIALOG_H

#include <stddef.h>

typedef enum hasciicam_gui_file_dialog_result {
    HASCIICAM_GUI_FILE_DIALOG_ERROR = -1,
    HASCIICAM_GUI_FILE_DIALOG_CANCELLED = 0,
    HASCIICAM_GUI_FILE_DIALOG_SELECTED = 1,
    HASCIICAM_GUI_FILE_DIALOG_NOT_AVAILABLE = 2
} hasciicam_gui_file_dialog_result;

typedef enum hasciicam_gui_file_dialog_kind {
    HASCIICAM_GUI_FILE_DIALOG_TOML = 0,
    HASCIICAM_GUI_FILE_DIALOG_IMAGE
} hasciicam_gui_file_dialog_kind;

/**
 * Open a system file dialog for selecting a TOML config file.
 */
hasciicam_gui_file_dialog_result hasciicam_gui_select_file(hasciicam_gui_file_dialog_kind kind,
                                                            char *out_path,
                                                            size_t out_path_size,
                                                            char *err,
                                                            size_t err_size);

/* Compatibility convenience for the existing configuration action. */
hasciicam_gui_file_dialog_result hasciicam_gui_select_toml_file(char *out_path,
                                                                size_t out_path_size,
                                                                char *err,
                                                                size_t err_size);

#endif
