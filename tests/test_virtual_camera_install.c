#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../src/virtual_camera/windows/install/hasciicam_virtual_camera_install.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static int ends_with(const wchar_t *text, const wchar_t *suffix) {
    size_t text_len;
    size_t suffix_len;

    if (text == NULL || suffix == NULL)
        return 0;
    text_len = wcslen(text);
    suffix_len = wcslen(suffix);
    if (suffix_len > text_len)
        return 0;
    return wcscmp(text + (text_len - suffix_len), suffix) == 0;
}

int main(void) {
    wchar_t root[256];
    wchar_t dll_path[256];
    wchar_t registry_key[256];
    char err[128];

    expect_true(wcscmp(hasciicam_virtual_camera_install_clsid_string(),
                       L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}") == 0,
                "install helper should expose the stable CLSID string");

    expect_true(hasciicam_virtual_camera_install_default_root(root, sizeof(root) / sizeof(root[0]), err, sizeof(err)),
                "default install root should be buildable");
    if (hasciicam_virtual_camera_install_default_root(root, sizeof(root) / sizeof(root[0]), err, sizeof(err))) {
        expect_true(wcsstr(root, L"HasciiCam") != NULL,
                    "default install root should include the product name");
    }

    expect_true(hasciicam_virtual_camera_install_default_dll_path(dll_path, sizeof(dll_path) / sizeof(dll_path[0]), err, sizeof(err)),
                "default DLL path should be buildable");
    if (hasciicam_virtual_camera_install_default_dll_path(dll_path, sizeof(dll_path) / sizeof(dll_path[0]), err, sizeof(err))) {
        expect_true(ends_with(dll_path, L"hasciicam_virtual_camera_source.dll"),
                    "default DLL path should end with the source DLL name");
    }

    expect_true(hasciicam_virtual_camera_install_registry_key(registry_key, sizeof(registry_key) / sizeof(registry_key[0]), err, sizeof(err)),
                "registry key path should be buildable");
    if (hasciicam_virtual_camera_install_registry_key(registry_key, sizeof(registry_key) / sizeof(registry_key[0]), err, sizeof(err))) {
        expect_true(wcsstr(registry_key, L"Software\\Classes\\CLSID\\") == registry_key,
                    "registry key should use the CLSID class root");
        expect_true(wcsstr(registry_key, L"\\InprocServer32") != NULL,
                    "registry key should target InprocServer32");
    }

#ifdef NDEBUG
    expect_true(!hasciicam_virtual_camera_install_debug_logging_enabled(),
                "release install should disable source trace logging");
#else
    expect_true(hasciicam_virtual_camera_install_debug_logging_enabled(),
                "debug install should enable source trace logging");
#endif

#ifdef _WIN32
    {
        wchar_t module_path[MAX_PATH];
        wchar_t module_dir[MAX_PATH];
        wchar_t source_path[MAX_PATH];
        wchar_t temp_root[MAX_PATH];
        wchar_t dest_dir[MAX_PATH];
        wchar_t dest_path[MAX_PATH];
        wchar_t removable_path[MAX_PATH];
        wchar_t *slash;
        DWORD length;

        length = GetModuleFileNameW(NULL, module_path, (DWORD)(sizeof(module_path) / sizeof(module_path[0])));
        expect_true(length > 0 && length < (DWORD)(sizeof(module_path) / sizeof(module_path[0])),
                    "test executable path should be available");
        if (length > 0 && length < (DWORD)(sizeof(module_path) / sizeof(module_path[0]))) {
            wcscpy(module_dir, module_path);
            slash = wcsrchr(module_dir, L'\\');
            expect_true(slash != NULL, "test executable path should contain a directory separator");
            if (slash != NULL) {
                *slash = L'\0';
                expect_true(swprintf(source_path,
                                     sizeof(source_path) / sizeof(source_path[0]),
                                     L"%ls\\hasciicam_virtual_camera_source.dll",
                                     module_dir) >= 0,
                            "source DLL path formatting should succeed");
                if (swprintf(source_path,
                             sizeof(source_path) / sizeof(source_path[0]),
                             L"%ls\\hasciicam_virtual_camera_source.dll",
                             module_dir) >= 0) {
                    wcscpy(dll_path, source_path);
                    expect_true(GetFileAttributesW(dll_path) != INVALID_FILE_ATTRIBUTES,
                                "source DLL should exist next to the test binary");
                }
            }
        }
        length = GetTempPathW((DWORD)(sizeof(temp_root) / sizeof(temp_root[0])), temp_root);
        expect_true(length > 0 && length < (DWORD)(sizeof(temp_root) / sizeof(temp_root[0])),
                    "temporary directory should be available");
        if (length > 0 && length < (DWORD)(sizeof(temp_root) / sizeof(temp_root[0]))) {
            expect_true(swprintf(dest_dir,
                                 sizeof(dest_dir) / sizeof(dest_dir[0]),
                                 L"%lsHasciiCamInstallTest_%lu",
                                 temp_root,
                                 (unsigned long)GetCurrentProcessId()) >= 0,
                        "temporary install root formatting should succeed");
            if (swprintf(dest_dir,
                         sizeof(dest_dir) / sizeof(dest_dir[0]),
                         L"%lsHasciiCamInstallTest_%lu",
                         temp_root,
                         (unsigned long)GetCurrentProcessId()) >= 0) {
                expect_true(swprintf(dest_path,
                                     sizeof(dest_path) / sizeof(dest_path[0]),
                                     L"%ls\\hasciicam_virtual_camera_source.dll",
                                     dest_dir) >= 0,
                            "temporary DLL path formatting should succeed");
                if (swprintf(dest_path,
                             sizeof(dest_path) / sizeof(dest_path[0]),
                             L"%ls\\hasciicam_virtual_camera_source.dll",
                             dest_dir) >= 0) {
                    int copied = hasciicam_virtual_camera_install_copy_dll(dll_path,
                                                                           dest_path,
                                                                           err,
                                                                           sizeof(err));
                    if (!copied)
                        fprintf(stderr, "copy helper error: %s\n", err);
                    expect_true(copied, "copy helper should set permissions on a writable temp root");
                    expect_true(swprintf(removable_path,
                                         sizeof(removable_path) / sizeof(removable_path[0]),
                                         L"%ls\\removable.dll",
                                         temp_root) >= 0,
                                "removable DLL path formatting should succeed");
                    if (CopyFileW(dll_path, removable_path, FALSE)) {
                        expect_true(hasciicam_virtual_camera_install_remove_dll(
                                        removable_path, err, sizeof(err)),
                                    "remove helper should delete an unused source DLL");
                    } else {
                        expect_true(0, "removable DLL test file should be created");
                    }
                    expect_true(hasciicam_virtual_camera_install_remove_dll(
                                    removable_path, err, sizeof(err)),
                                "remove helper should accept an absent source DLL");
                    DeleteFileW(dest_path);
                    RemoveDirectoryW(dest_dir);
                }
            }
        }
    }
#endif

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
