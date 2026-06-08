#include <stdio.h>
#include <string.h>

#include "../src/app/app_virtual_camera.h"
#include "../src/gui/gui_bridge.h"
#include "../src/virtual_camera/virtual_camera_internal.h"

static int failures = 0;
static int register_calls = 0;
static int unregister_calls = 0;
static aa_context *seen_context = NULL;
static void *seen_user_data = NULL;
static hasciicam_sdl_frame_callback seen_callback = NULL;

static int failing_publish(hasciicam_virtual_camera_device *device,
                           const hasciicam_virtual_camera_frame *frame) {
    (void)device;
    (void)frame;
    return 0;
}

static void fake_close(hasciicam_virtual_camera_device *device) {
    (void)device;
}

static const char *fake_backend_name(void) {
    return "test";
}

static const hasciicam_virtual_camera_ops failing_ops = {
    failing_publish,
    fake_close,
    fake_backend_name
};

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static int fake_set_callback(aa_context *context,
                             hasciicam_sdl_frame_callback callback,
                             void *user_data) {
    if (callback != NULL) {
        register_calls++;
        seen_context = context;
        seen_user_data = user_data;
        seen_callback = callback;
    } else {
        unregister_calls++;
        seen_callback = NULL;
    }
    return 1;
}

int main(void) {
    hasciicam_app_virtual_camera vc;
    hasciicam_config cfg;
    char err[128];
    aa_context *dummy_context = (aa_context *)0x1;

    hasciicam_app_virtual_camera_init(&vc);
    hasciicam_config_init_defaults(&cfg);
    cfg.virtual_camera = 1;
    cfg.virtual_camera_width = 1280;
    cfg.virtual_camera_height = 720;
    cfg.virtual_camera_fps = 30;
    strcpy(cfg.virtual_camera_device, "HasciiCam");

    {
        char context[256];
        hasciicam_app_virtual_camera_format_context(&cfg, context, sizeof(context));
        expect_true(strstr(context, "backend=") != NULL, "diagnostic context should include backend");
        expect_true(strstr(context, "size=1280x720") != NULL, "diagnostic context should include size");
        expect_true(strstr(context, "fps=30") != NULL, "diagnostic context should include fps");
    }

#if defined(__linux__)
    expect_true(!hasciicam_app_virtual_camera_start(&vc,
                                                    dummy_context,
                                                    &cfg,
                                                    fake_set_callback,
                                                    err,
                                                    sizeof(err)),
                "start should fail cleanly without a V4L2 loopback device");
    expect_true(vc.active == 0, "controller should remain inactive after backend failure");
    expect_true(vc.device == NULL, "backend failure should not retain a device");
    expect_true(register_calls == 0, "backend failure should not register a frame callback");
#else
    expect_true(hasciicam_app_virtual_camera_start(&vc,
                                                  dummy_context,
                                                  &cfg,
                                                  fake_set_callback,
                                                  err,
                                                  sizeof(err)),
                "start should succeed with a fake SDL callback hook");
    expect_true(vc.active == 1, "controller should be active after start");
    expect_true(vc.device != NULL, "controller should own a virtual camera device");
    expect_true(hasciicam_app_virtual_camera_request(&vc) != NULL,
                "controller should retain the request");
    expect_true(hasciicam_app_virtual_camera_request(&vc)->width == 1280,
                "request width should be stored");
    expect_true(strcmp(hasciicam_app_virtual_camera_backend_name(&vc), "") != 0,
                "backend name should be available");
    expect_true(register_calls == 1, "frame callback should be registered exactly once");
    expect_true(seen_context == dummy_context, "callback registration should use the SDL context");
    expect_true(seen_user_data == &vc, "callback registration should hand back the controller");

    {
        hasciicam_virtual_camera_frame frame;
        unsigned char pixels[4] = {0, 0, 0, 0};
        memset(&frame, 0, sizeof(frame));
        frame.pixels = pixels;
        frame.width = 1;
        frame.height = 1;
        frame.stride_bytes = 4;
        frame.pixel_format = HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32;
        frame.timestamp_100ns = 1000000ULL;
        expect_true(seen_callback != NULL, "callback should be captured after start");
        seen_callback(seen_user_data, &frame);
        expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 1ULL,
                    "first frame should be accepted");
        expect_true(hasciicam_app_virtual_camera_dropped_frames(&vc) == 0ULL,
                    "first frame should not be dropped");
        frame.timestamp_100ns = 1000000ULL + 1ULL;
        seen_callback(seen_user_data, &frame);
        expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 1ULL,
                    "early frame should be rate-limited");
        expect_true(hasciicam_app_virtual_camera_dropped_frames(&vc) == 1ULL,
                    "early frame should be counted as dropped");
        frame.timestamp_100ns = 1000000ULL + (10000000ULL / 30ULL);
        seen_callback(seen_user_data, &frame);
        expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 2ULL,
                    "frame at the interval boundary should be accepted");
        frame.timestamp_100ns = 1000000ULL + (10000000ULL / 30ULL) +
                                (10000000ULL / 45ULL);
        seen_callback(seen_user_data, &frame);
        expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 2ULL,
                    "early uneven-cadence frame should be dropped");
        frame.timestamp_100ns = 1000000ULL + 2ULL * (10000000ULL / 30ULL) +
                                1ULL;
        seen_callback(seen_user_data, &frame);
        expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 3ULL,
                    "deadline accumulator should preserve the requested average cadence");

        hasciicam_virtual_camera_close(vc.device);
        vc.device = hasciicam_virtual_camera_device_create(
            &failing_ops, 1, "test", NULL, err, sizeof(err));
        frame.timestamp_100ns += 10000000ULL / 30ULL;
        {
            unsigned long long last_publish = vc.last_publish_100ns;
            seen_callback(seen_user_data, &frame);
            expect_true(hasciicam_app_virtual_camera_accepted_frames(&vc) == 3ULL,
                        "failed publish should not be counted as accepted");
            expect_true(hasciicam_app_virtual_camera_dropped_frames(&vc) == 3ULL,
                        "failed publish should be counted as dropped");
            expect_true(vc.last_publish_100ns == last_publish,
                        "failed publish should not advance the publish deadline");
        }
    }

    hasciicam_app_virtual_camera_stop(&vc, fake_set_callback);
    expect_true(vc.active == 0, "controller should be inactive after stop");
    expect_true(vc.device == NULL, "device should be released after stop");
    expect_true(unregister_calls == 1, "frame callback should be unregistered exactly once");
#endif

#if defined(__linux__)
    {
        hasciicam_app_virtual_camera linux_vc;
        hasciicam_config linux_cfg;
        char linux_err[128];

        hasciicam_app_virtual_camera_init(&linux_vc);
        hasciicam_config_init_defaults(&linux_cfg);
        linux_cfg.virtual_camera = 1;
        strcpy(linux_cfg.device, "/dev/video10");
        strcpy(linux_cfg.virtual_camera_device, "/dev/video10");
        expect_true(!hasciicam_app_virtual_camera_start(&linux_vc,
                                                        dummy_context,
                                                        &linux_cfg,
                                                        fake_set_callback,
                                                        linux_err,
                                                        sizeof(linux_err)),
                    "virtual camera should reject sharing the capture device path");
    }
#endif

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
