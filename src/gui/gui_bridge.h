#ifndef HASCIICAM_GUI_BRIDGE_H
#define HASCIICAM_GUI_BRIDGE_H

#include <aalib.h>
#include "gui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hasciicam_virtual_camera_frame;
typedef void (*hasciicam_sdl_frame_callback)(void *user_data,
                                             const struct hasciicam_virtual_camera_frame *frame);

int hasciicam_sdl_set_gui_state(aa_context *context, hasciicam_gui_state *state);
int hasciicam_sdl_set_runtime_colors(aa_context *context,
                                     unsigned int foreground_rgb,
                                     unsigned int background_rgb,
                                     int aa_dimmer);
int hasciicam_sdl_set_runtime_font(aa_context *context, const char *font_short_name);
/** Resize the SDL output to a character grid, fitting it to the display. */
int hasciicam_sdl_set_grid_size(aa_context *context, int width, int height);
int hasciicam_sdl_set_frame_callback(aa_context *context,
                                     hasciicam_sdl_frame_callback callback,
                                     void *user_data);

#ifdef __cplusplus
}
#endif

#endif
