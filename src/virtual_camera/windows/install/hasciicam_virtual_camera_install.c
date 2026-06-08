#include "hasciicam_virtual_camera_install.h"

#ifdef _WIN32

#include <windows.h>
#include <sddl.h>
#include <shlobj.h>
#include <stdio.h>

static const wchar_t kHasciiCamInstallRootName[] = L"HasciiCam";
static const wchar_t kHasciiCamInstallDllName[] = L"hasciicam_virtual_camera_source.dll";
static const wchar_t kHasciiCamInstallClsid[] = L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}";
static const wchar_t kHasciiCamInstallDirectorySddl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;LS)";
static const wchar_t kHasciiCamInstallFileSddl[] = L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;LS)";

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    snprintf(err, err_size, "%s", msg != NULL ? msg : "unknown error");
}

static int join_path(wchar_t *out, size_t out_size, const wchar_t *left, const wchar_t *right) {
    size_t left_len;
    size_t right_len;
    size_t needed;

    if (out == NULL || left == NULL || right == NULL || out_size == 0)
        return 0;
    left_len = wcslen(left);
    right_len = wcslen(right);
    needed = left_len + 1 + right_len + 1;
    if (needed > out_size)
        return 0;
    wcscpy(out, left);
    if (left_len > 0 && out[left_len - 1] != L'\\' && out[left_len - 1] != L'/') {
        out[left_len] = L'\\';
        out[left_len + 1] = L'\0';
        left_len++;
    }
    wcscat(out, right);
    return 1;
}

static int ensure_directory(const wchar_t *path) {
    int rc;

    if (path == NULL)
        return 0;
    rc = SHCreateDirectoryExW(NULL, path, NULL);
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_EXISTS || rc == ERROR_ALREADY_EXISTS;
}

static int apply_sddl_to_path(const wchar_t *path,
                              const wchar_t *sddl,
                              char *err,
                              size_t err_size) {
    PSECURITY_DESCRIPTOR security_descriptor = NULL;

    if (path == NULL || sddl == NULL) {
        set_error(err, err_size, "path and security descriptor are required");
        return 0;
    }
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,
                                                              SDDL_REVISION_1,
                                                              &security_descriptor,
                                                              NULL)) {
        set_error(err, err_size, "unable to build security descriptor");
        return 0;
    }
    if (!SetFileSecurityW(path, DACL_SECURITY_INFORMATION, security_descriptor)) {
        LocalFree(security_descriptor);
        set_error(err, err_size, "unable to set access permissions");
        return 0;
    }
    LocalFree(security_descriptor);
    return 1;
}

const wchar_t *hasciicam_virtual_camera_install_clsid_string(void) {
    return kHasciiCamInstallClsid;
}

int hasciicam_virtual_camera_install_default_root(wchar_t *out,
                                                  size_t out_size,
                                                  char *err,
                                                  size_t err_size) {
    wchar_t program_files[MAX_PATH];
    DWORD len;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "output buffer is required");
        return 0;
    }
    len = GetEnvironmentVariableW(L"ProgramFiles", program_files, (DWORD)(sizeof(program_files) / sizeof(program_files[0])));
    if (len == 0 || len >= sizeof(program_files) / sizeof(program_files[0])) {
        set_error(err, err_size, "unable to locate Program Files");
        return 0;
    }
    if (!join_path(out, out_size, program_files, kHasciiCamInstallRootName)) {
        set_error(err, err_size, "install root path is too long");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_install_default_dll_path(wchar_t *out,
                                                      size_t out_size,
                                                      char *err,
                                                      size_t err_size) {
    wchar_t root[MAX_PATH];

    if (!hasciicam_virtual_camera_install_default_root(root, sizeof(root) / sizeof(root[0]), err, err_size))
        return 0;
    if (!join_path(out, out_size, root, kHasciiCamInstallDllName)) {
        set_error(err, err_size, "DLL install path is too long");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_install_registry_key(wchar_t *out,
                                                  size_t out_size,
                                                  char *err,
                                                  size_t err_size) {
    wchar_t clsid[64];
    size_t needed;

    if (out == NULL || out_size == 0) {
        set_error(err, err_size, "output buffer is required");
        return 0;
    }
    clsid[0] = L'\0';
    needed = wcslen(L"Software\\Classes\\CLSID\\") + wcslen(hasciicam_virtual_camera_install_clsid_string()) + wcslen(L"\\InprocServer32") + 1;
    if (needed > out_size) {
        set_error(err, err_size, "registry key path is too long");
        return 0;
    }
    wcscpy(out, L"Software\\Classes\\CLSID\\");
    wcscat(out, hasciicam_virtual_camera_install_clsid_string());
    wcscat(out, L"\\InprocServer32");
    return 1;
}

int hasciicam_virtual_camera_install_copy_dll(const wchar_t *source_path,
                                              const wchar_t *dest_path,
                                              char *err,
                                              size_t err_size) {
    wchar_t *dest_dir_end;
    wchar_t dest_dir[MAX_PATH];

    if (source_path == NULL || dest_path == NULL) {
        set_error(err, err_size, "source and destination paths are required");
        return 0;
    }
    if (wcslen(dest_path) >= MAX_PATH) {
        set_error(err, err_size, "destination path is too long");
        return 0;
    }
    wcscpy(dest_dir, dest_path);
    dest_dir_end = wcsrchr(dest_dir, L'\\');
    if (dest_dir_end == NULL) {
        set_error(err, err_size, "destination path is invalid");
        return 0;
    }
    *dest_dir_end = L'\0';
    if (!ensure_directory(dest_dir)) {
        set_error(err, err_size, "unable to create install directory");
        return 0;
    }
    if (!CopyFileW(source_path, dest_path, FALSE)) {
        set_error(err, err_size, "unable to copy DLL into install root");
        return 0;
    }
    if (!apply_sddl_to_path(dest_dir, kHasciiCamInstallDirectorySddl, err, err_size))
        return 0;
    if (!apply_sddl_to_path(dest_path, kHasciiCamInstallFileSddl, err, err_size))
        return 0;
    return 1;
}

static int write_registry_string(HKEY root, const wchar_t *subkey, const wchar_t *name, const wchar_t *value) {
    HKEY key = NULL;
    LONG rc;

    rc = RegCreateKeyExW(root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS)
        return 0;
    rc = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

int hasciicam_virtual_camera_install_register(const wchar_t *dll_path,
                                              char *err,
                                              size_t err_size) {
    wchar_t key_path[256];

    if (dll_path == NULL) {
        set_error(err, err_size, "DLL path is required");
        return 0;
    }
    if (!hasciicam_virtual_camera_install_registry_key(key_path, sizeof(key_path) / sizeof(key_path[0]), err, err_size))
        return 0;
    if (!write_registry_string(HKEY_LOCAL_MACHINE, key_path, NULL, dll_path)) {
        set_error(err, err_size, "unable to write COM registration");
        return 0;
    }
    if (!write_registry_string(HKEY_LOCAL_MACHINE, key_path, L"ThreadingModel", L"Both")) {
        set_error(err, err_size, "unable to write threading model");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_install_unregister(char *err,
                                                size_t err_size) {
    wchar_t key_path[256];
    LONG rc;

    if (!hasciicam_virtual_camera_install_registry_key(key_path, sizeof(key_path) / sizeof(key_path[0]), err, err_size))
        return 0;
    rc = RegDeleteTreeW(HKEY_LOCAL_MACHINE, key_path);
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
        set_error(err, err_size, "unable to remove COM registration");
        return 0;
    }
    return 1;
}

int hasciicam_virtual_camera_install_status(const wchar_t *dll_path,
                                            int *dll_exists,
                                            int *registered,
                                            char *err,
                                            size_t err_size) {
    wchar_t key_path[256];
    LONG rc;
    HKEY key = NULL;

    if (dll_exists != NULL)
        *dll_exists = 0;
    if (registered != NULL)
        *registered = 0;
    if (dll_path == NULL) {
        set_error(err, err_size, "DLL path is required");
        return 0;
    }
    if (GetFileAttributesW(dll_path) != INVALID_FILE_ATTRIBUTES) {
        if (dll_exists != NULL)
            *dll_exists = 1;
    }
    if (!hasciicam_virtual_camera_install_registry_key(key_path, sizeof(key_path) / sizeof(key_path[0]), err, err_size))
        return 0;
    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, KEY_QUERY_VALUE, &key);
    if (rc == ERROR_SUCCESS) {
        if (registered != NULL)
            *registered = 1;
        RegCloseKey(key);
    }
    return 1;
}

#endif
