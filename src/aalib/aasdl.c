#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#ifdef SDL_DRIVER
#include <SDL2/SDL.h>
#include "aalib.h"
#include "aaint.h"
#include "aasdlint.h"
#include "../gui/gui_bridge.h"
#include "../render/render_font.h"
#if defined(HASCIICAM_ENABLE_GUI)
#include "../gui/gui_overlay.h"
#endif

__AA_CONST struct aa_driver SDL_d;
extern int quiet;

#ifndef HASCIICAM_APP_TITLE
#define HASCIICAM_APP_TITLE "hasciicam"
#endif

static void SDL_flush(aa_context *c);
static void SDL_process_events(struct sdldriverdata *d);
static void SDL_set_fullscreen(struct sdldriverdata *d, int enable);
static void SDL_apply_runtime_colors(struct sdldriverdata *d,
                                     unsigned int foreground_rgb,
                                     unsigned int background_rgb);

static int SDL_env_is(const char *value, const char *expected)
{
    return value != NULL && expected != NULL && SDL_strcasecmp(value, expected) == 0;
}

static Uint32 SDL_renderer_flags_from_env(void)
{
    const char *renderer = getenv("HASCIICAM_SDL_RENDERER");
    const char *vsync = getenv("HASCIICAM_SDL_VSYNC");
    Uint32 flags = SDL_RENDERER_ACCELERATED;

    if (SDL_env_is(renderer, "software")) {
        flags = SDL_RENDERER_SOFTWARE;
    } else if (SDL_env_is(renderer, "auto")) {
        flags = 0;
    } else if (SDL_env_is(renderer, "accelerated")) {
        flags = SDL_RENDERER_ACCELERATED;
    }

    if (vsync == NULL || SDL_env_is(vsync, "on") || SDL_env_is(vsync, "1") ||
        SDL_env_is(vsync, "true") || SDL_env_is(vsync, "yes")) {
        flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    return flags;
}

static int SDL_fullscreen_from_env(void)
{
    const char *fullscreen = getenv("HASCIICAM_SDL_FULLSCREEN");
    return SDL_env_is(fullscreen, "1") ||
           SDL_env_is(fullscreen, "on") ||
           SDL_env_is(fullscreen, "true") ||
           SDL_env_is(fullscreen, "yes");
}

static void SDL_log_renderer_info(SDL_Renderer *renderer, Uint32 requested_flags)
{
    SDL_RendererInfo info;
    if (renderer == NULL || quiet)
        return;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        fprintf(stderr,
                "SDL renderer: %s (requested:%s%s%s)\n",
                info.name ? info.name : "unknown",
                (requested_flags & SDL_RENDERER_SOFTWARE) ? " software" :
                    ((requested_flags & SDL_RENDERER_ACCELERATED) ? " accelerated" : " auto"),
                (requested_flags & SDL_RENDERER_PRESENTVSYNC) ? " vsync" : " no-vsync",
                (info.flags & SDL_RENDERER_TARGETTEXTURE) ? " target-texture" : "");
    }
}

static void SDL_destroy_stream_texture(struct sdldriverdata *d)
{
    if (d == NULL)
        return;
    if (d->stream_texture != NULL) {
        SDL_DestroyTexture(d->stream_texture);
        d->stream_texture = NULL;
    }
    if (d->stream_pixels != NULL) {
        free(d->stream_pixels);
        d->stream_pixels = NULL;
    }
    d->stream_width = 0;
    d->stream_height = 0;
}

static int SDL_ensure_stream_texture(struct sdldriverdata *d, int width, int height)
{
    size_t pixel_count;

    if (d == NULL || d->renderer == NULL || width <= 0 || height <= 0)
        return 0;
    if (d->stream_texture != NULL &&
        d->stream_pixels != NULL &&
        d->stream_width == width &&
        d->stream_height == height) {
        return 1;
    }

    SDL_destroy_stream_texture(d);

    d->stream_texture = SDL_CreateTexture(d->renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          width,
                                          height);
    if (d->stream_texture == NULL) {
        fprintf(stderr, "SDL_CreateTexture(streaming) failed: %s\n", SDL_GetError());
        return 0;
    }

    pixel_count = (size_t)width * (size_t)height;
    d->stream_pixels = calloc(pixel_count, sizeof(*d->stream_pixels));
    if (d->stream_pixels == NULL) {
        fprintf(stderr, "SDL_flush: Failed to allocate streaming pixels\n");
        SDL_destroy_stream_texture(d);
        return 0;
    }

    d->stream_width = width;
    d->stream_height = height;
    d->force_clear = 1;
    return 1;
}

static Uint32 SDL_map_rgb(struct sdldriverdata *d, int color)
{
    return SDL_MapRGB(d->stream_format,
                      (color >> 16) & 0xFF,
                      (color >> 8) & 0xFF,
                      color & 0xFF);
}

static void SDL_stream_draw_char(struct sdldriverdata *d,
                                 unsigned char ch,
                                 int cell_x,
                                 int cell_y,
                                 int fg_color,
                                 int bg_color)
{
    int px;
    int py;
    int left;
    int top;
    int font_height;
    Uint32 fg;
    Uint32 bg;

    if (d == NULL || d->stream_pixels == NULL || d->font == NULL || d->font->data == NULL)
        return;

    left = cell_x * d->char_width;
    top = cell_y * d->char_height;
    fg = SDL_map_rgb(d, fg_color);
    bg = SDL_map_rgb(d, bg_color);

    for (py = 0; py < d->char_height; py++) {
        Uint32 *row = d->stream_pixels + ((top + py) * d->stream_width) + left;
        for (px = 0; px < d->char_width; px++)
            row[px] = bg;
    }

    font_height = d->font->height < d->char_height ? d->font->height : d->char_height;
    for (py = 0; py < font_height; py++) {
        int index = ch * d->font->height + py;
        unsigned char byte = d->font->data[index];
        Uint32 *row = d->stream_pixels + ((top + py) * d->stream_width) + left;
        for (px = 0; px < 8 && px < d->char_width; px++) {
            if (byte & (0x80 >> px))
                row[px] = fg;
        }
    }
}

static void SDL_set_fullscreen(struct sdldriverdata *d, int enable)
{
    Uint32 mode;
    if (d == NULL || d->window == NULL)
        return;
    mode = enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(d->window, mode) == 0) {
        d->fullscreen = enable ? 1 : 0;
    }
}

static void SDL_process_events(struct sdldriverdata *d)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (d != NULL && d->gui_ready) {
            hasciicam_gui_overlay_process_event(&event);
        }
        if (event.type == SDL_QUIT) {
            raise(SIGINT);
            return;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE) {
            raise(SIGINT);
            return;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_RIGHT &&
            d != NULL && d->gui_ready) {
            if (!hasciicam_gui_overlay_wants_mouse()) {
                d->gui_visible = d->gui_visible ? 0 : 1;
            }
            continue;
        }
        if (event.type == SDL_KEYDOWN && d != NULL) {
            SDL_Keycode sym = event.key.keysym.sym;
            SDL_Keymod mod = event.key.keysym.mod;
            if (d->gui_ready && d->gui_visible && hasciicam_gui_overlay_wants_keyboard()) {
                continue;
            }
            if (sym == SDLK_q ||
                ((mod & KMOD_CTRL) && (sym == SDLK_q || sym == SDLK_c))) {
                raise(SIGINT);
                return;
            }
            if (sym == SDLK_f) {
                SDL_set_fullscreen(d, d->fullscreen ? 0 : 1);
                continue;
            }
            if (sym == SDLK_ESCAPE && d->fullscreen) {
                SDL_set_fullscreen(d, 0);
                continue;
            }
        }
    }
}

static void SDL_render_char(struct sdldriverdata *d, unsigned char ch, int x, int y, int fg_color, int bg_color)
{
    if (!d || !d->renderer) {
        fprintf(stderr, "SDL_render_char: d or renderer is NULL\n");
        return;
    }

    // Debug print
    // fprintf(stderr, "SDL_render_char: ch=%d, x=%d, y=%d\n", ch, x, y);

    SDL_Rect dst = {
        d->x_offset_px + (x * d->char_width),
        d->y_offset_px + (y * d->char_height),
        d->char_width,
        d->char_height
    };

    /* Always clear the cell background for dirty updates. */
    SDL_SetRenderDrawColor(d->renderer,
        (bg_color >> 16) & 0xFF,
        (bg_color >> 8) & 0xFF,
        bg_color & 0xFF, 255);
    SDL_RenderFillRect(d->renderer, &dst);

    if (d->font_texture) {
        SDL_Rect src = {
            (ch % 16) * d->char_width,
            (ch / 16) * d->char_height,
            d->char_width,
            d->char_height
        };
        SDL_SetTextureColorMod(d->font_texture,
            (fg_color >> 16) & 0xFF,
            (fg_color >> 8) & 0xFF,
            fg_color & 0xFF);
        SDL_RenderCopy(d->renderer, d->font_texture, &src, &dst);
        return;
    }

    /* Fallback path: draw character using font bitmap directly - pixel by pixel */
    if (d->font && d->font->data && ch < 256) {
        const unsigned char *font_data = d->font->data;
        int font_height = d->font->height < d->char_height ? d->font->height : d->char_height;

        SDL_SetRenderDrawColor(d->renderer,
            (fg_color >> 16) & 0xFF,
            (fg_color >> 8) & 0xFF,
            fg_color & 0xFF, 255);

        for (int py = 0; py < font_height; py++) {
            // Check bounds before accessing font_data
            int index = ch * d->font->height + py;
            if (index >= 0 && index < (256 * d->font->height)) {
                unsigned char byte = font_data[index];
                for (int px = 0; px < 8 && px < d->char_width; px++) {
                    if (byte & (0x80 >> px)) {
                        SDL_Rect pixel = { dst.x + px, dst.y + py, 1, 1 };
                        SDL_RenderFillRect(d->renderer, &pixel);
                    }
                }
            }
        }
    }
}

static void SDL_create_font_texture(struct sdldriverdata *d)
{
    const int FONT_WIDTH = 8;
    const int FONT_HEIGHT = d->font ? d->font->height : 16;
    const int FONT_COLS = 16;
    const int FONT_ROWS = 16;
    
    d->char_width = FONT_WIDTH;
    d->char_height = FONT_HEIGHT;
    
    int tex_width = FONT_WIDTH * FONT_COLS;
    int tex_height = FONT_HEIGHT * FONT_ROWS;
    
    /* Use RGBA8888 format explicitly for SDL2/SDL3 compatibility */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, tex_width, tex_height, 32, 
        SDL_PIXELFORMAT_RGBA8888);
    
    if (!surface) {
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        return;
    }
    
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    
    const unsigned char *font_data = d->font->data;
    int font_height = d->font->height;
    
    for (int ch = 0; ch < 256; ch++) {
        int col = ch % FONT_COLS;
        int row = ch / FONT_COLS;
        
        for (int y = 0; y < font_height && y < FONT_HEIGHT; y++) {
            unsigned char byte = font_data[ch * font_height + y];
            for (int x = 0; x < FONT_WIDTH; x++) {
                if (byte & (0x80 >> x)) {
                    int px = col * FONT_WIDTH + x;
                    int py = row * FONT_HEIGHT + y;
                    Uint32 *pixels = (Uint32 *)surface->pixels;
                    pixels[py * tex_width + px] = SDL_MapRGBA(surface->format, 255, 255, 255, 255);
                }
            }
        }
    }
    
    d->font_texture = SDL_CreateTextureFromSurface(d->renderer, surface);
    if (d->font_texture) {
        SDL_SetTextureBlendMode(d->font_texture, SDL_BLENDMODE_BLEND);
    } else {
        fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(surface);
}

static int SDL_init(__AA_CONST struct aa_hardware_params *p, __AA_CONST void *none,
                    struct aa_hardware_params *dest, void **driverdata)
{
    static __AA_CONST struct aa_hardware_params def = {
        NULL, AA_DIM_MASK | AA_REVERSE_MASK | AA_NORMAL_MASK | AA_BOLD_MASK | AA_BOLDFONT_MASK | AA_EXTENDED,
        0, 0,
        0, 0,
        80, 25,
        0, 0,
        0, 0,
        0.0, 0.0
    };
    
    (void)none;
    
    struct sdldriverdata *d;
    
    fflush(stderr);
    
    /* Check if SDL video subsystem is already initialized */
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
            return 0;
        }
    }
    
    *dest = def;
    *driverdata = d = calloc(1, sizeof(*d));
    
    if (!d) {
        fprintf(stderr, "SDL driver: Failed to allocate memory\n");
        SDL_Quit();
        return 0;
    }
    
    d->width = p->width ? p->width : (p->recwidth ? p->recwidth : 80);
    d->height = p->height ? p->height : (p->recheight ? p->recheight : 25);
    
    if (p->maxwidth && d->width > p->maxwidth)
        d->width = p->maxwidth;
    if (p->minwidth && d->width < p->minwidth)
        d->width = p->minwidth;
    if (p->maxheight && d->height > p->maxheight)
        d->height = p->maxheight;
    if (p->minheight && d->height < p->minheight)
        d->height = p->minheight;
    
    d->font = p->font ? p->font : &aa_font16;
    dest->font = d->font;
    
    d->char_width = 8;
    d->char_height = d->font->height;
    
    int win_width = d->width * d->char_width;
    int win_height = d->height * d->char_height;
    SDL_Rect usable_bounds;

    /* Keep initial window inside primary usable display bounds. */
    if (SDL_GetDisplayUsableBounds(0, &usable_bounds) == 0 &&
        usable_bounds.w > 0 && usable_bounds.h > 0) {
        if (win_width > usable_bounds.w)
            win_width = usable_bounds.w;
        if (win_height > usable_bounds.h)
            win_height = usable_bounds.h;
    }
    
    d->window = SDL_CreateWindow(HASCIICAM_APP_TITLE,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        win_width, win_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!d->window) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        free(d);
        SDL_Quit();
        return 0;
    }

    if (SDL_fullscreen_from_env())
        SDL_set_fullscreen(d, 1);
    
    /* Get actual window size (may differ due to DPI scaling/WM constraints) */
    int actual_width, actual_height;
    SDL_GetWindowSize(d->window, &actual_width, &actual_height);
    d->width = actual_width / d->char_width;
    d->height = actual_height / d->char_height;
    if (d->width < 1)
        d->width = 1;
    if (d->height < 1)
        d->height = 1;
    
    /* Update dest params to match actual size */
    dest->width = d->width;
    dest->height = d->height;

    if (!quiet) {
        fprintf(stderr,
                "SDL window size: %dx%d px (cell %dx%d px, grid %dx%d chars)\n",
                actual_width, actual_height,
                d->char_width, d->char_height,
                d->width, d->height);
    }
    
    Uint32 renderer_flags = SDL_renderer_flags_from_env();
    d->renderer = SDL_CreateRenderer(d->window, -1, renderer_flags);
    if (!d->renderer) {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(d->window);
        free(d);
        SDL_Quit();
        return 0;
    }
    SDL_log_renderer_info(d->renderer, renderer_flags);

    d->stream_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    if (d->stream_format == NULL) {
        fprintf(stderr, "SDL_AllocFormat failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(d->renderer);
        SDL_DestroyWindow(d->window);
        free(d);
        SDL_Quit();
        return 0;
    }
    
    SDL_create_font_texture(d);
    
    d->black_color = 0x000000;
    d->dim_color = 0x686868;
    d->normal_color = 0xB2B2B2;
    d->bold_color = 0xFFFFFF;
    d->special_color = 0x0000FF;
    
    d->inverted = 0;
    d->cvisible = 0;
    d->fullscreen = 0;
    d->gui_ready = 0;
    d->gui_visible = 0;
    d->gui_state = NULL;
    d->x_offset_px = 0;
    d->y_offset_px = 0;
    d->force_clear = 1;
    d->Xpos = 0;
    d->Ypos = 0;
    
    d->previoust = NULL;
    d->previousa = NULL;
    
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);
    SDL_RenderPresent(d->renderer);
    
    aa_recommendlowkbd("X11");
    
    fflush(stderr);
    return 1;
}

static void SDL_uninit(aa_context *c)
{
    struct sdldriverdata *d = c->driverdata;
    
    if (!d)
        return;
    
    if (d->gui_ready) {
        hasciicam_gui_overlay_shutdown();
        d->gui_ready = 0;
    }

    if (d->previoust) {
        free(d->previoust);
        d->previoust = NULL;
    }
    if (d->previousa) {
        free(d->previousa);
        d->previousa = NULL;
    }
    if (d->font_texture) {
        SDL_DestroyTexture(d->font_texture);
        d->font_texture = NULL;
    }
    SDL_destroy_stream_texture(d);
    if (d->stream_format) {
        SDL_FreeFormat(d->stream_format);
        d->stream_format = NULL;
    }
    if (d->renderer) {
        SDL_DestroyRenderer(d->renderer);
        d->renderer = NULL;
    }
    if (d->window) {
        SDL_DestroyWindow(d->window);
        d->window = NULL;
    }
    
    free(d);
    c->driverdata = NULL;
    
    /* Only quit SDL if video subsystem is still initialized */
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Quit();
    }
}

static void SDL_getsize(aa_context *c, int *width, int *height)
{
    struct sdldriverdata *d = c->driverdata;
    int win_width, win_height;

    SDL_GetWindowSize(d->window, &win_width, &win_height);

    int new_width = win_width / d->char_width;
    int new_height = win_height / d->char_height;

    // Ensure minimum size of 1x1
    if (new_width < 1) new_width = 1;
    if (new_height < 1) new_height = 1;

    if (new_width != d->width || new_height != d->height) {
        d->width = new_width;
        d->height = new_height;
        d->force_clear = 1;

        if (d->previoust) {
            free(d->previoust);
            free(d->previousa);
            d->previoust = NULL;
            d->previousa = NULL;
        }
    }

    *width = d->width;
    *height = d->height;
}

static void SDL_setattr(aa_context *c, int attr)
{
    struct sdldriverdata *d = c->driverdata;
    d->current_attr = attr;
}

static void SDL_print(aa_context *c, __AA_CONST char *text)
{
    struct sdldriverdata *d = c->driverdata;
    int len = (int)strlen(text);
    
    for (int i = 0; i < len; i++) {
        if (d->Xpos >= d->width) {
            d->Xpos = 0;
            d->Ypos++;
            if (d->Ypos >= d->height)
                d->Ypos = 0;
        }
        d->Xpos++;
    }
}

static void SDL_gotoxy(aa_context *c, int x, int y)
{
    struct sdldriverdata *d = c->driverdata;
    d->Xpos = x;
    d->Ypos = y;
}

static void SDL_flush(aa_context *c)
{
    struct sdldriverdata *d = c->driverdata;
    int pos;
    int draw_w_chars;
    int draw_h_chars;
    int win_width;
    int win_height;
    int content_px_w;
    int content_px_h;
    int new_x_offset;
    int new_y_offset;

    SDL_process_events(d);

    int scrwidth = aa_scrwidth(c);
    int scrheight = aa_scrheight(c);
    int bufsize = scrwidth * scrheight;
    
    draw_w_chars = scrwidth < d->width ? scrwidth : d->width;
    draw_h_chars = scrheight < d->height ? scrheight : d->height;

    SDL_GetWindowSize(d->window, &win_width, &win_height);
    content_px_w = draw_w_chars * d->char_width;
    content_px_h = draw_h_chars * d->char_height;
    new_x_offset = (win_width - content_px_w) / 2;
    new_y_offset = (win_height - content_px_h) / 2;
    if (new_x_offset < 0)
        new_x_offset = 0;
    if (new_y_offset < 0)
        new_y_offset = 0;

    if (new_x_offset != d->x_offset_px || new_y_offset != d->y_offset_px) {
        d->x_offset_px = new_x_offset;
        d->y_offset_px = new_y_offset;
        d->force_clear = 1;
        if (d->previoust && d->previousa) {
            memset(d->previoust, 0xFF, bufsize);
            memset(d->previousa, 0xFF, bufsize);
        }
    }

    /* Allocate change-tracking buffers based on actual screen size */
    if (!d->previoust) {
        d->previoust = malloc(bufsize);
        d->previousa = malloc(bufsize);
        if (!d->previoust || !d->previousa) {
            fprintf(stderr, "SDL_flush: Failed to allocate buffers\n");
            free(d->previoust);
            free(d->previousa);
            d->previoust = NULL;
            d->previousa = NULL;
            return;
        }
        memset(d->previoust, 0xFF, bufsize);
        memset(d->previousa, 0xFF, bufsize);
        d->force_clear = 1;
    }

    if (!SDL_ensure_stream_texture(d, content_px_w, content_px_h))
        return;

    if (d->force_clear) {
        memset(d->previoust, 0xFF, bufsize);
        memset(d->previousa, 0xFF, bufsize);
    }

    for (int y = 0; y < draw_h_chars; y++) {
        for (int x = 0; x < draw_w_chars; x++) {
            pos = x + y * scrwidth;
            
            unsigned char ch = c->textbuffer[pos];
            int attr = c->attrbuffer[pos];
            
            int fg_color = d->normal_color;
            int bg_color = d->black_color;
            
            if (ch != d->previoust[pos] || attr != d->previousa[pos]) {
                SDL_stream_draw_char(d, ch, x, y, fg_color, bg_color);
                d->previoust[pos] = ch;
                d->previousa[pos] = attr;
            }
        }
    }

    if (SDL_UpdateTexture(d->stream_texture,
                          NULL,
                          d->stream_pixels,
                          d->stream_width * (int)sizeof(*d->stream_pixels)) != 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);
    {
        SDL_Rect dst = {
            d->x_offset_px,
            d->y_offset_px,
            content_px_w,
            content_px_h
        };
        SDL_RenderCopy(d->renderer, d->stream_texture, NULL, &dst);
    }
    if (d->cvisible) {
        SDL_SetRenderDrawColor(d->renderer, 255, 255, 255, 255);
        SDL_Rect cursor_rect = {
            d->x_offset_px + (d->Xpos * d->char_width),
            d->y_offset_px + ((d->Ypos + 1) * d->char_height) - 2,
            d->char_width,
            2
        };
        SDL_RenderFillRect(d->renderer, &cursor_rect);
    }

    if (d->gui_ready && d->gui_visible && d->gui_state != NULL) {
        hasciicam_gui_overlay_new_frame();
        hasciicam_gui_overlay_draw(d->gui_state);
        hasciicam_gui_overlay_render(d->renderer);
    }
    SDL_RenderPresent(d->renderer);
    d->force_clear = 0;
}

static void SDL_cursor(aa_context *c, int mode)
{
    struct sdldriverdata *d = c->driverdata;
    d->cvisible = mode;
}

__AA_CONST struct aa_driver SDL_d = {
    "SDL", "SDL2 driver 1.0",
    SDL_init,
    SDL_uninit,
    SDL_getsize,
    SDL_setattr,
    NULL,
    SDL_gotoxy,
    SDL_flush,
    SDL_cursor
};

static unsigned int clamp_rgb24(unsigned int rgb) {
    return rgb & 0x00FFFFFFu;
}

static void SDL_apply_runtime_colors(struct sdldriverdata *d,
                                     unsigned int foreground_rgb,
                                     unsigned int background_rgb) {
    if (d == NULL)
        return;
    foreground_rgb = clamp_rgb24(foreground_rgb);
    background_rgb = clamp_rgb24(background_rgb);
    d->normal_color = (int)foreground_rgb;
    d->black_color = (int)background_rgb;
    d->dim_color = d->normal_color;
    d->bold_color = d->normal_color;
    d->special_color = d->normal_color;
    d->force_clear = 1;
}

int hasciicam_sdl_set_gui_state(aa_context *context, struct hasciicam_gui_state *state) {
    struct sdldriverdata *d;
    if (context == NULL || context->driverdata == NULL)
        return 0;
    if (context->driver != &SDL_d)
        return 0;
    d = (struct sdldriverdata *)context->driverdata;
    d->gui_state = state;
#if defined(HASCIICAM_ENABLE_GUI)
    if (!d->gui_ready) {
        d->gui_ready = hasciicam_gui_overlay_init(d->window, d->renderer) ? 1 : 0;
    }
#else
    d->gui_ready = 0;
#endif
    return d->gui_ready;
}

int hasciicam_sdl_set_runtime_colors(aa_context *context, unsigned int foreground_rgb, unsigned int background_rgb) {
    struct sdldriverdata *d;
    if (context == NULL || context->driverdata == NULL)
        return 0;
    if (context->driver != &SDL_d)
        return 0;
    d = (struct sdldriverdata *)context->driverdata;
    SDL_apply_runtime_colors(d, foreground_rgb, background_rgb);
    return 1;
}

int hasciicam_sdl_set_runtime_font(aa_context *context, const char *font_short_name) {
    struct sdldriverdata *d;
    hasciicam_font_desc desc;
    int win_width;
    int win_height;
    if (context == NULL || context->driverdata == NULL || font_short_name == NULL)
        return 0;
    if (context->driver != &SDL_d)
        return 0;
    desc = hasciicam_font_find(font_short_name);
    if (desc.font == NULL)
        return 0;
    d = (struct sdldriverdata *)context->driverdata;
    if (d->font == desc.font)
        return 1;

    aa_setfont(context, desc.font);
    d->font = desc.font;
    d->char_width = 8;
    d->char_height = desc.height;
    if (d->font_texture != NULL) {
        SDL_DestroyTexture(d->font_texture);
        d->font_texture = NULL;
    }
    SDL_create_font_texture(d);
    SDL_destroy_stream_texture(d);
    if (d->previoust != NULL) {
        free(d->previoust);
        d->previoust = NULL;
    }
    if (d->previousa != NULL) {
        free(d->previousa);
        d->previousa = NULL;
    }
    SDL_GetWindowSize(d->window, &win_width, &win_height);
    d->width = win_width / d->char_width;
    d->height = win_height / d->char_height;
    if (d->width < 1)
        d->width = 1;
    if (d->height < 1)
        d->height = 1;
    aa_resize(context);
    d->force_clear = 1;
    return 1;
}

#endif
