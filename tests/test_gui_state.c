#include <stdio.h>
#include <string.h>

#include "../src/gui/gui_state.h"

static int expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    hasciicam_config cfg;
    hasciicam_gui_state state;
    unsigned char sample[6] = {0, 64, 128, 192, 224, 255};
    unsigned int rgb = 0;

    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.background, "123456");
    strcpy(cfg.foreground, "ABCDEF");
    cfg.aa_bright = 77;
    cfg.aa_contrast = 8;
    cfg.aa_gamma = 2;
    cfg.invert = 1;
    cfg.mirror_x = 1;
    cfg.mirror_y = 0;
    strcpy(cfg.font, "vga16");

    hasciicam_gui_state_init(&state, &cfg);
    if (!expect_true(state.aa_bright == 77, "bright init failed")) return 1;
    if (!expect_true(state.aa_contrast == 8, "contrast init failed")) return 1;
    if (!expect_true((int)state.aa_gamma == 2, "gamma init failed")) return 1;
    if (!expect_true(state.invert == 1, "invert init failed")) return 1;
    if (!expect_true(state.mirror_x == 1 && state.mirror_y == 0, "mirror init failed")) return 1;
    if (!expect_true(state.background_rgb == 0x123456u, "background parse failed")) return 1;
    if (!expect_true(state.foreground_rgb == 0xABCDEFu, "foreground parse failed")) return 1;
    if (!expect_true(strcmp(state.font, "vga16") == 0, "font init failed")) return 1;
    if (!expect_true(strcmp(state.active_font, "vga16") == 0, "active font init failed")) return 1;

    if (!expect_true(hasciicam_gui_parse_rgb_hex("00ff11", &rgb) == 1, "rgb parse lower failed")) return 1;
    if (!expect_true(rgb == 0x00FF11u, "rgb parse value failed")) return 1;
    if (!expect_true(hasciicam_gui_parse_rgb_hex("bad", &rgb) == 0, "rgb parse invalid failed")) return 1;

    hasciicam_gui_format_rgb_hex(0x00AA01u, cfg.background, sizeof(cfg.background));
    if (!expect_true(strcmp(cfg.background, "00AA01") == 0, "rgb format failed")) return 1;

    state.aa_bright = 66;
    state.aa_contrast = 7;
    state.aa_gamma = 3.4f;
    state.invert = 0;
    state.mirror_x = 0;
    state.mirror_y = 1;
    state.background_rgb = 0x0000FFu;
    state.foreground_rgb = 0xFF0000u;
    strcpy(state.font, "vga8");
    hasciicam_gui_state_copy_to_config(&state, &cfg);

    if (!expect_true(cfg.aa_bright == 66, "bright copy failed")) return 1;
    if (!expect_true(cfg.aa_contrast == 7, "contrast copy failed")) return 1;
    if (!expect_true(cfg.aa_gamma == 3, "gamma copy failed")) return 1;
    if (!expect_true(cfg.invert == 0, "invert copy failed")) return 1;
    if (!expect_true(cfg.mirror_x == 0 && cfg.mirror_y == 1, "mirror copy failed")) return 1;
    if (!expect_true(strcmp(cfg.background, "0000FF") == 0, "background copy failed")) return 1;
    if (!expect_true(strcmp(cfg.foreground, "FF0000") == 0, "foreground copy failed")) return 1;
    if (!expect_true(strcmp(cfg.font, "vga8") == 0, "font copy failed")) return 1;

    if (!expect_true(hasciicam_gui_state_update_preview(&state, sample, 3, 2) == 1,
                     "preview update failed")) return 1;
    if (!expect_true(state.preview_width == 3 && state.preview_height == 2,
                     "preview size failed")) return 1;
    if (!expect_true(state.preview_gray != NULL && state.preview_gray[1] == 64 && state.preview_gray[5] == 255,
                     "preview copy failed")) return 1;
    hasciicam_gui_state_reset_preview(&state);
    if (!expect_true(state.preview_gray == NULL && state.preview_capacity == 0,
                     "preview reset failed")) return 1;

    return 0;
}
