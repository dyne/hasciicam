#if !defined(SDL_DRIVER)

#include "gui_bridge.h"

int hasciicam_sdl_set_gui_state(aa_context *context, hasciicam_gui_state *state) {
    (void)context;
    (void)state;
    return 0;
}

int hasciicam_sdl_set_runtime_colors(aa_context *context, unsigned int foreground_rgb, unsigned int background_rgb) {
    (void)context;
    (void)foreground_rgb;
    (void)background_rgb;
    return 0;
}

#endif
