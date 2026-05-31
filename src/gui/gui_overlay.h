#ifndef HASCIICAM_GUI_OVERLAY_H
#define HASCIICAM_GUI_OVERLAY_H

#include <SDL2/SDL.h>
#include "gui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

int hasciicam_gui_overlay_init(SDL_Window *window, SDL_Renderer *renderer);
void hasciicam_gui_overlay_shutdown(void);
void hasciicam_gui_overlay_new_frame(void);
int hasciicam_gui_overlay_process_event(const SDL_Event *event);
void hasciicam_gui_overlay_draw(hasciicam_gui_state *state);
void hasciicam_gui_overlay_render(SDL_Renderer *renderer);
int hasciicam_gui_overlay_wants_mouse(void);
int hasciicam_gui_overlay_wants_keyboard(void);

#ifdef __cplusplus
}
#endif

#endif
