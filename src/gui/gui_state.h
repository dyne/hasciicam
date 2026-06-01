#ifndef HASCIICAM_GUI_STATE_H
#define HASCIICAM_GUI_STATE_H

#include <stddef.h>

#include "../app/app_config.h"
#include "../capture/capture.h"

typedef struct hasciicam_gui_state {
    int visible;

    int aa_bright;
    int aa_contrast;
    float aa_gamma;
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
    capture_pixel_format capture_pixel_format;

    char save_path[260];
    char load_path[260];
    char status_message[256];
    int status_is_error;

    int save_requested;
    int load_requested;
    int open_load_dialog_requested;
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

/**
 * Parse a 6-digit RGB hex string into 0xRRGGBB.
 */
int hasciicam_gui_parse_rgb_hex(const char *text, unsigned int *out_rgb);

/**
 * Format 0xRRGGBB into a 6-digit uppercase hex string.
 */
void hasciicam_gui_format_rgb_hex(unsigned int rgb, char *out, size_t out_size);

#endif
