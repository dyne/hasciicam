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
    hasciicam_virtual_camera_device *device = NULL;
    hasciicam_virtual_camera_frame frame;
    char err[128];
    int w = 0;
    int h = 0;
    int rect_x = 0;
    int rect_y = 0;
    int rect_w = 0;
    int rect_h = 0;
    unsigned char pixel[4] = {1, 2, 3, 4};
    unsigned char src[16] = {
        0, 0, 255, 255,
        0, 255, 0, 255,
        255, 0, 0, 255,
        255, 255, 255, 255
    };
    unsigned char yuy2[8 * 2];
    unsigned char nv12[4 * 2 + 4];
    size_t yuy2_size = 0;
    size_t nv12_size = 0;

    hasciicam_virtual_camera_request_init(&request);
    expect_true(request.enabled == 0, "default enabled should be off");
    expect_true(request.width == 1280 && request.height == 720, "default size should be 1280x720");
    expect_true(request.fps == 30, "default fps should be 30");
    expect_true(hasciicam_virtual_camera_default_backend_name()[0] != '\0',
                "default backend name should be available");

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

    request.enabled = 0;
    expect_true(hasciicam_virtual_camera_open_default(&device, &request, err, sizeof(err)),
                "open_default should return a fallback device");
    expect_true(device != NULL, "device should be allocated");
    expect_true(hasciicam_virtual_camera_is_supported(device) == 0,
                "fallback device should report unsupported");
    expect_true(strcmp(hasciicam_virtual_camera_backend_name(device), "unsupported") == 0,
                "backend name should be unsupported");

    memset(&frame, 0, sizeof(frame));
    frame.pixels = pixel;
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixel_format = HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32;
    frame.timestamp_100ns = 123;
    expect_true(hasciicam_virtual_camera_publish(device, &frame),
                "fallback publish should succeed");
    hasciicam_virtual_camera_close(device);

    yuy2_size = hasciicam_virtual_camera_yuy2_size(4, 2, 0);
    expect_true(yuy2_size == 16, "yuy2 size should be width*2*height");
    nv12_size = hasciicam_virtual_camera_nv12_size(4, 2, 0, 0);
    expect_true(nv12_size == 12, "nv12 size should be width*height*3/2");
    expect_true(hasciicam_virtual_camera_letterbox_rect(2, 2, 4, 2,
                                                        &rect_x,
                                                        &rect_y,
                                                        &rect_w,
                                                        &rect_h),
                "letterbox rectangle should be computable");
    expect_true(rect_x == 1 && rect_y == 0, "letterbox rectangle should be centered");
    expect_true(rect_w == 2 && rect_h == 2, "letterbox rectangle should preserve source aspect");
    memset(yuy2, 0xAA, sizeof(yuy2));
    expect_true(hasciicam_virtual_camera_scale_bgra32_to_yuy2(src, 2, 2, 8,
                                                              yuy2, 4, 2, 0,
                                                              0, 0),
                "yuy2 conversion should succeed");
    expect_true(yuy2[0] == 0x10 && yuy2[1] == 0x10, "left letterbox should stay black");
    expect_true(yuy2[2] == 82, "red y should be 82");
    expect_true(yuy2[4] == 144, "green y should be 144");
    expect_true(yuy2[6] == 0x10 && yuy2[7] == 0x10, "right letterbox should stay black");
    expect_true(yuy2[8] == 0x10 && yuy2[9] == 0x10, "lower letterbox should stay black");
    expect_true(yuy2[10] == 41, "blue y should be 41");
    expect_true(yuy2[12] == 235, "white y should be 235");

    memset(nv12, 0xAA, sizeof(nv12));
    expect_true(hasciicam_virtual_camera_scale_bgra32_to_nv12(src, 2, 2, 8,
                                                              nv12, 4, 2, 0, 0,
                                                              0, 0),
                "nv12 conversion should succeed");
    expect_true(nv12[0] == 0x10, "nv12 left letterbox should stay black");
    expect_true(nv12[1] == 82, "nv12 red y should be 82");
    expect_true(nv12[2] == 144, "nv12 green y should be 144");
    expect_true(nv12[3] == 0x10, "nv12 right letterbox should stay black");
    expect_true(nv12[4] == 0x10, "nv12 lower left letterbox should stay black");
    expect_true(nv12[5] == 41, "nv12 blue y should be 41");
    expect_true(nv12[6] == 235, "nv12 white y should be 235");
    expect_true(nv12[7] == 0x10, "nv12 lower right letterbox should stay black");
    expect_true(nv12[9] == 128 && nv12[10] == 128,
                "nv12 chroma should average to neutral for the test pattern");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
