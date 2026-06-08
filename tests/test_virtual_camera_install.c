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

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
