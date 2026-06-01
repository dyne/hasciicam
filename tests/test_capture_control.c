#include <stdio.h>
#include <string.h>

#include "../src/capture/capture_control.h"

static int g_set_called = 0;
static int g_set_auto_called = 0;

static int fake_list(capture_device *dev, capture_control_desc *out, int max_controls) {
    (void)dev;
    if (out == NULL || max_controls < 1)
        return 0;
    memset(out, 0, sizeof(*out));
    out[0].id = CAPTURE_CONTROL_BRIGHTNESS;
    out[0].name = "brightness";
    out[0].min_value = 0;
    out[0].max_value = 255;
    out[0].current_value = 128;
    out[0].writable = 1;
    return 1;
}

static int fake_set(capture_device *dev, capture_control_id id, int value) {
    (void)dev;
    if (id != CAPTURE_CONTROL_BRIGHTNESS || value != 140)
        return 0;
    g_set_called = 1;
    return 1;
}

static int fake_set_auto(capture_device *dev, capture_control_id id, int enabled) {
    (void)dev;
    if (id != CAPTURE_CONTROL_BRIGHTNESS || enabled != 1)
        return 0;
    g_set_auto_called = 1;
    return 1;
}

static int expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void) {
    capture_control_desc desc[4];
    capture_ops ops;

    memset(&ops, 0, sizeof(ops));
    memset(desc, 0, sizeof(desc));

    if (!expect_true(capture_controls_list((capture_device *)1, &ops, desc, 4) == 0,
                     "list should return zero without backend hook")) return 1;
    if (!expect_true(capture_control_set((capture_device *)1, &ops, CAPTURE_CONTROL_BRIGHTNESS, 1) == 0,
                     "set should fail without backend hook")) return 1;
    if (!expect_true(capture_control_set_auto((capture_device *)1, &ops, CAPTURE_CONTROL_BRIGHTNESS, 1) == 0,
                     "set_auto should fail without backend hook")) return 1;

    ops.list_controls = fake_list;
    ops.set_control = fake_set;
    ops.set_control_auto = fake_set_auto;

    if (!expect_true(capture_controls_list((capture_device *)1, &ops, desc, 4) == 1,
                     "list should report one control")) return 1;
    if (!expect_true(strcmp(desc[0].name, "brightness") == 0,
                     "control name should match")) return 1;
    if (!expect_true(capture_control_set((capture_device *)1, &ops, CAPTURE_CONTROL_BRIGHTNESS, 140) == 1,
                     "set should succeed")) return 1;
    if (!expect_true(capture_control_set_auto((capture_device *)1, &ops, CAPTURE_CONTROL_BRIGHTNESS, 1) == 1,
                     "set_auto should succeed")) return 1;
    if (!expect_true(g_set_called == 1 && g_set_auto_called == 1,
                     "hooks should be called")) return 1;

    printf("capture_control tests passed\n");
    return 0;
}
