#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../install/hasciicam_virtual_camera_install.h"

static void print_usage(void) {
    fprintf(stderr, "Usage: hasciicam-vcam install --source <dll> [--root <dir>]\n");
    fprintf(stderr, "       hasciicam-vcam status [--root <dir>]\n");
    fprintf(stderr, "       hasciicam-vcam remove [--root <dir>]\n");
}

static int utf8_to_wide(const char *input, wchar_t *out, size_t out_size) {
    int count;

    if (input == NULL || out == NULL || out_size == 0)
        return 0;
    count = MultiByteToWideChar(CP_UTF8, 0, input, -1, out, (int)out_size);
    return count > 0;
}

static int build_root_from_arg(const char *arg, wchar_t *out, size_t out_size, char *err, size_t err_size) {
    if (arg == NULL)
        return hasciicam_virtual_camera_install_default_root(out, out_size, err, err_size);
    if (!utf8_to_wide(arg, out, out_size)) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "install root is too long");
        return 0;
    }
    return 1;
}

static int build_dll_path(const wchar_t *root, wchar_t *out, size_t out_size, char *err, size_t err_size) {
    const wchar_t *name = L"hasciicam_virtual_camera_source.dll";
    size_t root_len;
    size_t name_len;

    root_len = wcslen(root);
    name_len = wcslen(name);
    if (root_len + 1 + name_len + 1 > out_size) {
        if (err != NULL && err_size > 0)
            snprintf(err, err_size, "install path is too long");
        return 0;
    }
    wcscpy(out, root);
    if (root_len == 0 || out[root_len - 1] != L'\\')
        wcscat(out, L"\\");
    wcscat(out, name);
    return 1;
}

int main(int argc, char **argv) {
    wchar_t root_w[512];
    wchar_t dll_path_w[512];
    wchar_t source_path_w[512];
    char err[256];
    const char *command = NULL;
    const char *root_arg = NULL;
    const char *source_arg = NULL;
    int i;
    int dll_exists = 0;
    int registered = 0;

    err[0] = '\0';
    if (argc < 2) {
        print_usage();
        return 1;
    }

    command = argv[1];
    if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0 || strcmp(command, "help") == 0) {
        print_usage();
        return 0;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
            source_arg = argv[++i];
            continue;
        }
    }

    if (strcmp(command, "status") == 0) {
        if (!build_root_from_arg(root_arg, root_w, sizeof(root_w) / sizeof(root_w[0]), err, sizeof(err)))
            goto fail;
        if (!build_dll_path(root_w, dll_path_w, sizeof(dll_path_w) / sizeof(dll_path_w[0]), err, sizeof(err)))
            goto fail;
        if (!hasciicam_virtual_camera_install_status(dll_path_w, &dll_exists, &registered, err, sizeof(err)))
            goto fail;
        printf("root=%ls\n", root_w);
        printf("dll=%ls\n", dll_path_w);
        printf("dll_exists=%d\nregistered=%d\n", dll_exists, registered);
        return (dll_exists && registered) ? 0 : 2;
    }

    if (strcmp(command, "remove") == 0) {
        if (!hasciicam_virtual_camera_install_unregister(err, sizeof(err)))
            goto fail;
        if (!build_root_from_arg(root_arg, root_w, sizeof(root_w) / sizeof(root_w[0]), err, sizeof(err)))
            goto fail;
        if (!build_dll_path(root_w, dll_path_w, sizeof(dll_path_w) / sizeof(dll_path_w[0]), err, sizeof(err)))
            goto fail;
        DeleteFileW(dll_path_w);
        printf("removed\n");
        return 0;
    }

    if (strcmp(command, "install") == 0) {
        if (source_arg == NULL) {
            snprintf(err, sizeof(err), "--source is required for install");
            goto fail;
        }
        if (!build_root_from_arg(root_arg, root_w, sizeof(root_w) / sizeof(root_w[0]), err, sizeof(err)))
            goto fail;
        if (!build_dll_path(root_w, dll_path_w, sizeof(dll_path_w) / sizeof(dll_path_w[0]), err, sizeof(err)))
            goto fail;
        if (!utf8_to_wide(source_arg, source_path_w, sizeof(source_path_w) / sizeof(source_path_w[0]))) {
            snprintf(err, sizeof(err), "source path is too long");
            goto fail;
        }
        if (!hasciicam_virtual_camera_install_copy_dll(source_path_w, dll_path_w, err, sizeof(err)))
            goto fail;
        if (!hasciicam_virtual_camera_install_register(dll_path_w, err, sizeof(err)))
            goto fail;
        printf("installed\n");
        return 0;
    }

    print_usage();
    return 1;

fail:
    if (err[0] != '\0')
        fprintf(stderr, "%s\n", err);
    return 1;
}

#endif
