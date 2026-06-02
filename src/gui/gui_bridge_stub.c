#if !defined(SDL_DRIVER)

#include "gui_bridge.h"

int hasciicam_sdl_set_gui_state(aa_context *context, hasciicam_gui_state *state) {
    (void)context;
    (void)state;
    return 0;
}

int hasciicam_sdl_set_runtime_colors(aa_context *context,
                                     unsigned int foreground_rgb,
                                     unsigned int background_rgb,
                                     int aa_dimmer) {
    (void)context;
    (void)foreground_rgb;
    (void)background_rgb;
    (void)aa_dimmer;
    return 0;
}

int hasciicam_sdl_set_runtime_font(aa_context *context, const char *font_short_name) {
    (void)context;
    (void)font_short_name;
    return 0;
}

#endif
