#include <stdio.h>

#if defined(__linux__)

#include <linux/videodev2.h>

#include "../src/virtual_camera/virtual_camera_v4l2.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

int main(void) {
    expect_true(!hasciicam_virtual_camera_v4l2_is_output_capable(0),
                "zero capabilities should not be output capable");
    expect_true(hasciicam_virtual_camera_v4l2_is_output_capable(V4L2_CAP_VIDEO_OUTPUT),
                "video output capability bit should be accepted");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}

#endif
