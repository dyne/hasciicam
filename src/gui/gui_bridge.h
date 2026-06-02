#ifndef HASCIICAM_GUI_BRIDGE_H
#define HASCIICAM_GUI_BRIDGE_H

#include <aalib.h>
#include "gui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

int hasciicam_sdl_set_gui_state(aa_context *context, hasciicam_gui_state *state);
int hasciicam_sdl_set_runtime_colors(aa_context *context,
                                     unsigned int foreground_rgb,
                                     unsigned int background_rgb,
                                     int aa_dimmer);
int hasciicam_sdl_set_runtime_font(aa_context *context, const char *font_short_name);

#ifdef __cplusplus
}
#endif

#endif
