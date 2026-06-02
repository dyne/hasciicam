#include "display_size.h"

#include <stddef.h>

#if defined(SDL_DRIVER)
#include <SDL.h>
#endif

int hasciicam_display_size_detect_primary(int *width, int *height) {
    if (width == NULL || height == NULL)
        return 0;

#if defined(SDL_DRIVER)
    int need_quit = 0;
    SDL_Rect bounds;
    SDL_DisplayMode mode;

    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
            return 0;
        need_quit = 1;
    }

    if (SDL_GetDisplayUsableBounds(0, &bounds) == 0 &&
        bounds.w > 0 && bounds.h > 0) {
        *width = bounds.w;
        *height = bounds.h;
        if (need_quit)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 1;
    }

    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 &&
        mode.w > 0 && mode.h > 0) {
        *width = mode.w;
        *height = mode.h;
        if (need_quit)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 1;
    }

    if (need_quit)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif

    return 0;
}
