#include "gui_state.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static unsigned int clamp_rgb(unsigned int rgb) {
    return rgb & 0x00FFFFFFu;
}

static int hex_value(int ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    ch = toupper(ch);
    if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}

int hasciicam_gui_parse_rgb_hex(const char *text, unsigned int *out_rgb) {
    unsigned int value = 0;
    int i;

    if (text == NULL || out_rgb == NULL)
        return 0;
    if (strlen(text) != 6)
        return 0;
    for (i = 0; i < 6; ++i) {
        int hv = hex_value((unsigned char)text[i]);
        if (hv < 0)
            return 0;
        value = (value << 4) | (unsigned int)hv;
    }
    *out_rgb = clamp_rgb(value);
    return 1;
}

void hasciicam_gui_format_rgb_hex(unsigned int rgb, char *out, size_t out_size) {
    if (out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%06X", clamp_rgb(rgb));
}

void hasciicam_gui_state_init(hasciicam_gui_state *state, const hasciicam_config *cfg) {
    unsigned int fg = 0x00FF00u;
    unsigned int bg = 0x000000u;

    if (state == NULL || cfg == NULL)
        return;
    memset(state, 0, sizeof(*state));

    state->aa_bright = cfg->aa_bright;
    state->aa_contrast = cfg->aa_contrast;
    state->aa_gamma = (float)cfg->aa_gamma;
    state->invert = cfg->invert ? 1 : 0;
    state->mirror_x = cfg->mirror_x ? 1 : 0;
    state->mirror_y = cfg->mirror_y ? 1 : 0;

    if (hasciicam_gui_parse_rgb_hex(cfg->foreground, &fg))
        state->foreground_rgb = fg;
    else
        state->foreground_rgb = 0x00FF00u;
    if (hasciicam_gui_parse_rgb_hex(cfg->background, &bg))
        state->background_rgb = bg;
    else
        state->background_rgb = 0x000000u;
    strncpy(state->font, cfg->font, sizeof(state->font) - 1);
    strncpy(state->active_font, cfg->font, sizeof(state->active_font) - 1);

    strncpy(state->save_path, "hasciicam.toml", sizeof(state->save_path) - 1);
    strncpy(state->load_path, "hasciicam.toml", sizeof(state->load_path) - 1);
}

void hasciicam_gui_state_copy_to_config(const hasciicam_gui_state *state, hasciicam_config *cfg) {
    if (state == NULL || cfg == NULL)
        return;
    cfg->aa_bright = state->aa_bright;
    cfg->aa_contrast = state->aa_contrast;
    cfg->aa_gamma = (int)(state->aa_gamma + 0.5f);
    cfg->invert = state->invert ? 1 : 0;
    cfg->mirror_x = state->mirror_x ? 1 : 0;
    cfg->mirror_y = state->mirror_y ? 1 : 0;
    strncpy(cfg->font, state->font, sizeof(cfg->font) - 1);
    cfg->font[sizeof(cfg->font) - 1] = '\0';
    hasciicam_gui_format_rgb_hex(state->background_rgb, cfg->background, sizeof(cfg->background));
    hasciicam_gui_format_rgb_hex(state->foreground_rgb, cfg->foreground, sizeof(cfg->foreground));
}

void hasciicam_gui_state_set_capture_info(hasciicam_gui_state *state, const capture_info *info) {
    if (state == NULL || info == NULL)
        return;
    state->capture_width = info->width;
    state->capture_height = info->height;
    state->capture_stride_bytes = info->stride_bytes;
    state->capture_pixel_format = info->pixel_format;
}

int hasciicam_gui_state_update_preview(hasciicam_gui_state *state,
                                       const unsigned char *gray_frame,
                                       int width,
                                       int height) {
    int size;
    if (state == NULL || gray_frame == NULL || width <= 0 || height <= 0)
        return 0;
    size = width * height;
    if (size <= 0)
        return 0;
    if (state->preview_capacity < size) {
        unsigned char *new_buf = (unsigned char *)malloc((size_t)size);
        if (new_buf == NULL)
            return 0;
        free(state->preview_gray);
        state->preview_gray = new_buf;
        state->preview_capacity = size;
    }
    memcpy(state->preview_gray, gray_frame, (size_t)size);
    state->preview_width = width;
    state->preview_height = height;
    state->preview_stride = width;
    state->preview_generation++;
    return 1;
}

void hasciicam_gui_state_reset_preview(hasciicam_gui_state *state) {
    if (state == NULL)
        return;
    free(state->preview_gray);
    state->preview_gray = NULL;
    state->preview_capacity = 0;
    state->preview_width = 0;
    state->preview_height = 0;
    state->preview_stride = 0;
    state->preview_generation = 0;
}
