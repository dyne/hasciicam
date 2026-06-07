#include <stdio.h>
#include <string.h>

#include "../src/app/app_virtual_camera.h"

static int failures = 0;
static int register_calls = 0;
static int unregister_calls = 0;
static aa_context *seen_context = NULL;
static void *seen_user_data = NULL;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static int fake_set_callback(aa_context *context,
                             void (*callback)(void *user_data, const struct hasciicam_virtual_camera_frame *frame),
                             void *user_data) {
    if (callback != NULL) {
        register_calls++;
        seen_context = context;
        seen_user_data = user_data;
    } else {
        unregister_calls++;
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

    expect_true(hasciicam_app_virtual_camera_start(&vc, dummy_context, &cfg, fake_set_callback, err, sizeof(err)),
                "start should succeed with a fake SDL callback hook");
    expect_true(vc.active == 1, "controller should be active after start");
    expect_true(vc.device != NULL, "controller should own a virtual camera device");
    expect_true(register_calls == 1, "frame callback should be registered exactly once");
    expect_true(seen_context == dummy_context, "callback registration should use the SDL context");
    expect_true(seen_user_data == &vc, "callback registration should hand back the controller");

    hasciicam_app_virtual_camera_stop(&vc, fake_set_callback);
    expect_true(vc.active == 0, "controller should be inactive after stop");
    expect_true(vc.device == NULL, "device should be released after stop");
    expect_true(unregister_calls == 1, "frame callback should be unregistered exactly once");

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
