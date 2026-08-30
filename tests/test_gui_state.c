#include <stdio.h>
#include <string.h>

#include "../src/gui/gui_state.h"
#include "../src/gui/gui_file_dialog.h"

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
    capture_control_desc controls[2];
    unsigned char sample[6] = {0, 64, 128, 192, 224, 255};
    unsigned int rgb = 0;

    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.background, "123456");
    strcpy(cfg.foreground, "ABCDEF");
    cfg.aa_bright = 77;
    cfg.aa_contrast = 8;
    cfg.aa_gamma = 2;
    cfg.aa_dimmer = 1;
    cfg.invert = 1;
    cfg.mirror_x = 1;
    cfg.mirror_y = 0;
    strcpy(cfg.font, "vga16");
    strcpy(cfg.image, "fixtures/source.png");

    hasciicam_gui_state_init(&state, &cfg);
    if (!expect_true(state.aa_bright == 77, "bright init failed")) return 1;
    if (!expect_true(state.aa_contrast == 8, "contrast init failed")) return 1;
    if (!expect_true((int)state.aa_gamma == 2, "gamma init failed")) return 1;
    if (!expect_true(state.aa_dimmer == 1, "aa_dimmer init failed")) return 1;
    if (!expect_true(state.invert == 1, "invert init failed")) return 1;
    if (!expect_true(state.mirror_x == 1 && state.mirror_y == 0, "mirror init failed")) return 1;
    if (!expect_true(state.background_rgb == 0x123456u, "background parse failed")) return 1;
    if (!expect_true(state.foreground_rgb == 0xABCDEFu, "foreground parse failed")) return 1;
    if (!expect_true(strcmp(state.font, "vga16") == 0, "font init failed")) return 1;
    if (!expect_true(strcmp(state.active_font, "vga16") == 0, "active font init failed")) return 1;
    if (!expect_true(strcmp(state.text_frame_path, "hasciicam.txt") == 0, "text frame path init failed")) return 1;
    if (!expect_true(strcmp(state.image_path, "fixtures/source.png") == 0, "image path init failed")) return 1;
    if (!expect_true(state.source_kind == HASCIICAM_GUI_SOURCE_IMAGE &&
                     strcmp(state.source_label, "Image") == 0, "image source init failed")) return 1;

    if (!expect_true(hasciicam_gui_parse_rgb_hex("00ff11", &rgb) == 1, "rgb parse lower failed")) return 1;
    if (!expect_true(rgb == 0x00FF11u, "rgb parse value failed")) return 1;
    if (!expect_true(hasciicam_gui_parse_rgb_hex("bad", &rgb) == 0, "rgb parse invalid failed")) return 1;

    hasciicam_gui_format_rgb_hex(0x00AA01u, cfg.background, sizeof(cfg.background));
    if (!expect_true(strcmp(cfg.background, "00AA01") == 0, "rgb format failed")) return 1;

    state.aa_bright = 66;
    state.aa_contrast = 7;
    state.aa_gamma = 3.4f;
    state.aa_dimmer = 0;
    state.invert = 0;
    state.mirror_x = 0;
    state.mirror_y = 1;
    state.background_rgb = 0x0000FFu;
    state.foreground_rgb = 0xFF0000u;
    strcpy(state.font, "vga8");
    strcpy(state.image_path, "replacement.jpg");
    hasciicam_gui_state_copy_to_config(&state, &cfg);

    if (!expect_true(cfg.aa_bright == 66, "bright copy failed")) return 1;
    if (!expect_true(cfg.aa_contrast == 7, "contrast copy failed")) return 1;
    if (!expect_true(cfg.aa_gamma == 3, "gamma copy failed")) return 1;
    if (!expect_true(cfg.aa_dimmer == 0, "aa_dimmer copy failed")) return 1;
    if (!expect_true(cfg.invert == 0, "invert copy failed")) return 1;
    if (!expect_true(cfg.mirror_x == 0 && cfg.mirror_y == 1, "mirror copy failed")) return 1;
    if (!expect_true(strcmp(cfg.background, "0000FF") == 0, "background copy failed")) return 1;
    if (!expect_true(strcmp(cfg.foreground, "FF0000") == 0, "foreground copy failed")) return 1;
    if (!expect_true(strcmp(cfg.font, "vga8") == 0, "font copy failed")) return 1;
    if (!expect_true(strcmp(cfg.image, "fixtures/source.png") == 0, "image source copy changed unexpectedly")) return 1;

    hasciicam_gui_state_set_source(&state, HASCIICAM_GUI_SOURCE_CAMERA, "Camera", "camera ready", 0);
    if (!expect_true(state.source_kind == HASCIICAM_GUI_SOURCE_CAMERA &&
                     strcmp(state.source_label, "Camera") == 0 &&
                     strcmp(state.status_message, "camera ready") == 0 && !state.status_is_error,
                     "camera source metadata failed")) return 1;
    state.load_image_requested = 1;
    state.use_camera_requested = 1;
    state.open_image_dialog_requested = 1;
    state.save_text_frame_requested = 1;
    if (!expect_true(state.load_image_requested && state.use_camera_requested &&
                     state.open_image_dialog_requested && state.save_text_frame_requested,
                     "independent source/output actions failed")) return 1;
    strcpy(state.text_frame_path, "custom-frame.txt");
    if (!expect_true(strcmp(state.text_frame_path, "custom-frame.txt") == 0,
                     "custom text frame path failed")) return 1;
    memset(state.image_path, 'x', sizeof(state.image_path));
    state.image_path[sizeof(state.image_path) - 1] = '\0';
    hasciicam_gui_state_copy_to_config(&state, &cfg);
    if (!expect_true(strcmp(cfg.image, "fixtures/source.png") == 0,
                     "image path should not change source config")) return 1;
    memset(cfg.image, 'x', sizeof(cfg.image));
    cfg.image[sizeof(cfg.image) - 1] = '\0';
    hasciicam_gui_state_init(&state, &cfg);
    if (!expect_true(state.image_path[sizeof(state.image_path) - 1] == '\0',
                     "image path init termination failed")) return 1;
    strcpy(cfg.image, "after-reinit.png");
    hasciicam_gui_state_init(&state, &cfg);
    if (!expect_true(strcmp(state.image_path, "after-reinit.png") == 0 &&
                     state.source_kind == HASCIICAM_GUI_SOURCE_IMAGE, "reinit image preservation failed")) return 1;

    if (!expect_true(hasciicam_gui_select_file(HASCIICAM_GUI_FILE_DIALOG_IMAGE,
                                               state.image_path, sizeof(state.image_path), NULL, 0) ==
                     HASCIICAM_GUI_FILE_DIALOG_NOT_AVAILABLE,
                     "fallback image dialog failed")) return 1;

    memset(controls, 0, sizeof(controls));
    controls[0].id = CAPTURE_CONTROL_BRIGHTNESS;
    controls[0].name = "brightness";
    controls[0].label = "Brightness";
    controls[0].min_value = 0;
    controls[0].max_value = 255;
    controls[0].current_value = 100;
    controls[0].writable = 1;
    hasciicam_gui_state_set_capture_controls(&state, controls, 1);
    if (!expect_true(state.capture_control_count == 1, "capture control count failed")) return 1;
    if (!expect_true(state.capture_controls[0].id == CAPTURE_CONTROL_BRIGHTNESS, "capture control id failed")) return 1;
    if (!expect_true(state.capture_controls[0].current_value == 100, "capture control copy failed")) return 1;

    if (!expect_true(hasciicam_gui_state_update_preview(&state, sample, 3, 2) == 1,
                     "preview update failed")) return 1;
    if (!expect_true(state.preview_width == 3 && state.preview_height == 2,
                     "preview size failed")) return 1;
    if (!expect_true(state.preview_gray != NULL && state.preview_gray[1] == 64 && state.preview_gray[5] == 255,
                     "preview copy failed")) return 1;
    hasciicam_gui_state_reset_preview(&state);
    if (!expect_true(state.preview_gray == NULL && state.preview_capacity == 0,
                     "preview reset failed")) return 1;

    hasciicam_gui_state_set_virtual_camera(&state,
                                           1,
                                           "linux-v4l2",
                                           "HasciiCam",
                                           "/dev/video10",
                                           1280,
                                           720,
                                           30,
                                           1,
                                           42ULL,
                                           7ULL);
    if (!expect_true(state.virtual_camera_enabled == 1, "virtual camera enabled failed")) return 1;
    if (!expect_true(strcmp(state.virtual_camera_backend, "linux-v4l2") == 0,
                     "virtual camera backend failed")) return 1;
    if (!expect_true(strcmp(state.virtual_camera_name, "HasciiCam") == 0,
                     "virtual camera name failed")) return 1;
    if (!expect_true(strcmp(state.virtual_camera_device, "/dev/video10") == 0,
                     "virtual camera device failed")) return 1;
    if (!expect_true(state.virtual_camera_width == 1280 && state.virtual_camera_height == 720,
                     "virtual camera size failed")) return 1;
    if (!expect_true(state.virtual_camera_fps == 30, "virtual camera fps failed")) return 1;
    if (!expect_true(state.virtual_camera_connected == 1, "virtual camera connected failed")) return 1;
    if (!expect_true(state.virtual_camera_accepted_frames == 42ULL,
                     "virtual camera accepted count failed")) return 1;
    if (!expect_true(state.virtual_camera_dropped_frames == 7ULL,
                     "virtual camera dropped count failed")) return 1;

    return 0;
}
