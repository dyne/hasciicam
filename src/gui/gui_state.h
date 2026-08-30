#ifndef HASCIICAM_GUI_STATE_H
#define HASCIICAM_GUI_STATE_H

#include <stddef.h>

#include "../app/app_config.h"
#include "../capture/capture.h"

typedef enum hasciicam_gui_source_kind {
    HASCIICAM_GUI_SOURCE_CAMERA = 0,
    HASCIICAM_GUI_SOURCE_IMAGE
} hasciicam_gui_source_kind;

typedef struct hasciicam_gui_state {
    int visible;

    int aa_bright;
    int aa_contrast;
    float aa_gamma;
    int aa_dimmer;
    int invert;

    int mirror_x;
    int mirror_y;

    unsigned int foreground_rgb;
    unsigned int background_rgb;
    char font[64];
    char active_font[64];
    int font_change_requested;

    int capture_width;
    int capture_height;
    int capture_stride_bytes;
    capture_pixel_format pixel_format;
    capture_control_desc capture_controls[CAPTURE_MAX_CONTROLS];
    int capture_control_count;
    int capture_control_change_requested;
    int capture_control_change_is_auto;
    capture_control_id capture_control_change_id;
    int capture_control_change_value;
    unsigned char *preview_gray;
    int preview_capacity;
    int preview_width;
    int preview_height;
    int preview_stride;
    unsigned int preview_generation;

    char save_path[260];
    char load_path[260];
    hasciicam_gui_source_kind source_kind;
    char source_label[64];
    char image_path[512];
    char status_message[256];
    int status_is_error;

    int virtual_camera_enabled;
    char virtual_camera_backend[32];
    char virtual_camera_name[64];
    char virtual_camera_device[256];
    int virtual_camera_width;
    int virtual_camera_height;
    int virtual_camera_fps;
    int virtual_camera_connected;
    unsigned long long virtual_camera_accepted_frames;
    unsigned long long virtual_camera_dropped_frames;

    int save_requested;
    int load_requested;
    int open_load_dialog_requested;
    int load_image_requested;
    int use_camera_requested;
    int open_image_dialog_requested;
} hasciicam_gui_state;

/**
 * Initialize runtime GUI state from launch config.
 */
void hasciicam_gui_state_init(hasciicam_gui_state *state, const hasciicam_config *cfg);

/**
 * Copy runtime GUI values back to the config snapshot.
 */
void hasciicam_gui_state_copy_to_config(const hasciicam_gui_state *state, hasciicam_config *cfg);

/**
 * Set capture info fields shown in the GUI panel.
 */
void hasciicam_gui_state_set_capture_info(hasciicam_gui_state *state, const capture_info *info);
void hasciicam_gui_state_set_source(hasciicam_gui_state *state,
                                    hasciicam_gui_source_kind kind,
                                    const char *label,
                                    const char *status,
                                    int status_is_error);
void hasciicam_gui_state_set_capture_controls(hasciicam_gui_state *state,
                                              const capture_control_desc *controls,
                                              int control_count);
void hasciicam_gui_state_set_virtual_camera(hasciicam_gui_state *state,
                                            int enabled,
                                            const char *backend,
                                            const char *name,
                                            const char *device,
                                            int width,
                                            int height,
                                            int fps,
                                            int connected,
                                            unsigned long long accepted_frames,
                                            unsigned long long dropped_frames);
int hasciicam_gui_state_update_preview(hasciicam_gui_state *state,
                                       const unsigned char *gray_frame,
                                       int width,
                                       int height);
void hasciicam_gui_state_reset_preview(hasciicam_gui_state *state);

/**
 * Parse a 6-digit RGB hex string into 0xRRGGBB.
 */
int hasciicam_gui_parse_rgb_hex(const char *text, unsigned int *out_rgb);

/**
 * Format 0xRRGGBB into a 6-digit uppercase hex string.
 */
void hasciicam_gui_format_rgb_hex(unsigned int rgb, char *out, size_t out_size);

#endif
