#include <stdio.h>
#include <string.h>

#include "../src/virtual_camera/virtual_camera.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

int main(void) {
    hasciicam_virtual_camera_request request;
    char err[128];
    int w = 0;
    int h = 0;

    hasciicam_virtual_camera_request_init(&request);
    expect_true(request.enabled == 0, "default enabled should be off");
    expect_true(request.width == 1280 && request.height == 720, "default size should be 1280x720");
    expect_true(request.fps == 30, "default fps should be 30");

    expect_true(hasciicam_virtual_camera_parse_size("1920x1080", &w, &h), "parse_size should accept lowercase x");
    expect_true(w == 1920 && h == 1080, "parse_size should return 1920x1080");
    expect_true(hasciicam_virtual_camera_parse_size("640X480", &w, &h), "parse_size should accept uppercase X");
    expect_true(w == 640 && h == 480, "parse_size should return 640x480");
    expect_true(!hasciicam_virtual_camera_parse_size("640", &w, &h), "parse_size should reject missing height");

    request.enabled = 1;
    request.width = 1280;
    request.height = 720;
    request.fps = 30;
    expect_true(hasciicam_virtual_camera_request_validate(&request, err, sizeof(err)),
                "validate should accept default request");

    request.width = 1279;
    expect_true(!hasciicam_virtual_camera_request_validate(&request, err, sizeof(err)),
                "validate should reject odd width");
    request.width = 1280;
    request.height = 719;
    expect_true(!hasciicam_virtual_camera_request_validate(&request, err, sizeof(err)),
                "validate should reject odd height");
    request.height = 720;
    request.fps = 0;
    expect_true(!hasciicam_virtual_camera_request_validate(&request, err, sizeof(err)),
                "validate should reject zero fps");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
