#if defined(_WIN32)

#include "gui_file_dialog.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>

hasciicam_gui_file_dialog_result hasciicam_gui_select_toml_file(char *out_path,
                                                                size_t out_path_size,
                                                                char *err,
                                                                size_t err_size) {
    OPENFILENAMEW ofn;
    WCHAR file_path[MAX_PATH];
    WCHAR filter[] = L"TOML files\0*.toml\0All files\0*.*\0\0";
    int utf8_len;

    if (out_path == NULL || out_path_size == 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "invalid output buffer");
        return HASCIICAM_GUI_FILE_DIALOG_ERROR;
    }

    memset(&ofn, 0, sizeof(ofn));
    memset(file_path, 0, sizeof(file_path));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file_path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        DWORD dlg_err = CommDlgExtendedError();
        if (dlg_err == 0)
            return HASCIICAM_GUI_FILE_DIALOG_CANCELLED;
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "file dialog failed (%lu)", (unsigned long)dlg_err);
        return HASCIICAM_GUI_FILE_DIALOG_ERROR;
    }

    utf8_len = WideCharToMultiByte(CP_UTF8, 0, file_path, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0 || (size_t)utf8_len > out_path_size) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "selected path is too long");
        return HASCIICAM_GUI_FILE_DIALOG_ERROR;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, file_path, -1, out_path, (int)out_path_size, NULL, NULL) <= 0) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "path conversion failed");
        return HASCIICAM_GUI_FILE_DIALOG_ERROR;
    }
    return HASCIICAM_GUI_FILE_DIALOG_SELECTED;
}

#endif
