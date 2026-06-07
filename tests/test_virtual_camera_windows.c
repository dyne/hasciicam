#include <stdio.h>
#include <wchar.h>

#include "../src/virtual_camera/windows/source/hasciicam_virtual_camera_source.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

int main(void) {
    const wchar_t *clsid = hasciicam_virtual_camera_source_clsid_string();
    expect_true(clsid != NULL, "clsid string should exist");
    expect_true(wcscmp(clsid, L"{29E1D0B1-0AF8-4D6F-9D5E-0F9A0F0D4F58}") == 0,
                "clsid string should match the declared source id");
    expect_true(hasciicam_virtual_camera_source_clsid() != NULL,
                "clsid pointer should exist");
    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
