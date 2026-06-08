#include <stdio.h>

#if defined(__linux__)

#include <linux/videodev2.h>
#include <errno.h>
#include <string.h>

#include "../src/virtual_camera/virtual_camera_v4l2.h"

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static void expect_contains(const char *text, const char *needle, const char *msg) {
    expect_true(text != NULL && needle != NULL && strstr(text, needle) != NULL, msg);
}

int main(void) {
    char err[128];

    expect_true(!hasciicam_virtual_camera_v4l2_is_output_capable(0),
                "zero capabilities should not be output capable");
    expect_true(hasciicam_virtual_camera_v4l2_is_output_capable(V4L2_CAP_VIDEO_OUTPUT),
                "video output capability bit should be accepted");
    expect_true(hasciicam_virtual_camera_v4l2_describe_error(EACCES, "open output device", err, sizeof(err)),
                "access denied message should format");
    expect_contains(err, "access denied", "access denied message should be classified");
    expect_true(hasciicam_virtual_camera_v4l2_describe_error(ENOENT, "open output device", err, sizeof(err)),
                "missing device message should format");
    expect_contains(err, "device absent", "missing device message should be classified");
    expect_true(hasciicam_virtual_camera_v4l2_describe_error(EINVAL, "querycap", err, sizeof(err)),
                "querycap failure message should format");
    expect_contains(err, "output node", "querycap failure should name the output node requirement");
    expect_true(hasciicam_virtual_camera_v4l2_should_retry_write(EINTR),
                "EINTR should be retried");
    expect_true(hasciicam_virtual_camera_v4l2_should_drop_frame(EAGAIN),
                "EAGAIN should drop the frame");
    expect_true(hasciicam_virtual_camera_v4l2_should_drop_frame(EWOULDBLOCK),
                "EWOULDBLOCK should drop the frame");
    expect_true(!hasciicam_virtual_camera_v4l2_should_disconnect_write(12, 12, 0),
                "full writes should not disconnect");
    expect_true(hasciicam_virtual_camera_v4l2_should_disconnect_write(8, 12, 0),
                "short writes should disconnect");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}

#endif
