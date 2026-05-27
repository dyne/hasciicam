#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#ifdef SDL_DRIVER
#include <SDL2/SDL.h>
#include "aalib.h"
#include "aaint.h"
#include "aasdlint.h"

__AA_CONST struct aa_driver SDL_d;
extern int quiet;

static void SDL_flush(aa_context *c);
static void SDL_process_events(void);

static void SDL_process_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            raise(SIGINT);
            return;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE) {
            raise(SIGINT);
            return;
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
        x * d->char_width,
        y * d->char_height,
        d->char_width,
        d->char_height
    };

    /* Draw background */
    if (bg_color != d->black_color) {
        SDL_SetRenderDrawColor(d->renderer,
            (bg_color >> 16) & 0xFF,
            (bg_color >> 8) & 0xFF,
            bg_color & 0xFF, 255);
        SDL_RenderFillRect(d->renderer, &dst);
    }

    /* Draw character using font bitmap directly - pixel by pixel */
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
    const int FONT_HEIGHT = 16;
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
    
    d->width = p->width ? p->width : 80;
    d->height = p->height ? p->height : 25;
    
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
    d->char_height = 16;
    
    int win_width = d->width * d->char_width;
    int win_height = d->height * d->char_height;
    
    d->window = SDL_CreateWindow("AA-lib SDL",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        win_width, win_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!d->window) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        free(d);
        SDL_Quit();
        return 0;
    }
    
    /* Get actual window size (may differ due to DPI scaling/WM constraints) */
    int actual_width, actual_height;
    SDL_GetWindowSize(d->window, &actual_width, &actual_height);
    d->width = actual_width / d->char_width;
    d->height = actual_height / d->char_height;
    
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
    
    d->renderer = SDL_CreateRenderer(d->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!d->renderer) {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(d->window);
        free(d);
        SDL_Quit();
        return 0;
    }
    
    /* Font texture not needed - we render directly */
    d->font_texture = NULL;
    
    d->black_color = 0x000000;
    d->dim_color = 0x686868;
    d->normal_color = 0xB2B2B2;
    d->bold_color = 0xFFFFFF;
    d->special_color = 0x0000FF;
    
    d->inverted = 0;
    d->cvisible = 1;
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
    int len = strlen(text);
    
    for (int i = 0; i < len; i++) {
        if (d->Xpos >= d->width) {
            d->Xpos = 0;
            d->Ypos++;
            if (d->Ypos >= d->height)
                d->Ypos = 0;
        }
        
        int fg_color, bg_color;
        
        if (d->inverted) {
            bg_color = d->bold_color;
            switch (d->current_attr) {
                case AA_DIM:      fg_color = d->dim_color; break;
                case AA_BOLD:     
                case AA_BOLDFONT: fg_color = d->black_color; break;
                case AA_REVERSE:  fg_color = d->bold_color; bg_color = d->black_color; break;
                case AA_SPECIAL:  fg_color = d->special_color; break;
                default:          fg_color = d->normal_color; break;
            }
        } else {
            bg_color = d->black_color;
            switch (d->current_attr) {
                case AA_DIM:      fg_color = d->dim_color; break;
                case AA_BOLD:     
                case AA_BOLDFONT: fg_color = d->bold_color; break;
                case AA_REVERSE:  fg_color = d->black_color; bg_color = d->normal_color; break;
                case AA_SPECIAL:  fg_color = d->special_color; break;
                default:          fg_color = d->normal_color; break;
            }
        }
        
        SDL_render_char(d, (unsigned char)text[i], d->Xpos, d->Ypos, fg_color, bg_color);
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

    SDL_process_events();

    int scrwidth = aa_scrwidth(c);
    int scrheight = aa_scrheight(c);
    int bufsize = scrwidth * scrheight;
    
    /* Allocate change-tracking buffers based on actual screen size */
    if (!d->previoust) {
        d->previoust = malloc(bufsize);
        d->previousa = malloc(bufsize);
        if (!d->previoust || !d->previousa) {
            fprintf(stderr, "SDL_flush: Failed to allocate buffers\n");
            return;
        }
        memset(d->previoust, ' ', bufsize);
        memset(d->previousa, 0, bufsize);
    }
    
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);
    
    for (int y = 0; y < scrheight && y < d->height; y++) {
        for (int x = 0; x < scrwidth && x < d->width; x++) {
            pos = x + y * scrwidth;
            
            unsigned char ch = c->textbuffer[pos];
            int attr = c->attrbuffer[pos];
            
            int fg_color, bg_color;
            
            if (d->inverted) {
                bg_color = d->bold_color;
                switch (attr) {
                    case AA_DIM:      fg_color = d->dim_color; break;
                    case AA_BOLD:     
                    case AA_BOLDFONT: fg_color = d->black_color; break;
                    case AA_REVERSE:  fg_color = d->bold_color; bg_color = d->black_color; break;
                    case AA_SPECIAL:  fg_color = d->special_color; break;
                    default:          fg_color = d->normal_color; break;
                }
            } else {
                bg_color = d->black_color;
                switch (attr) {
                    case AA_DIM:      fg_color = d->dim_color; break;
                    case AA_BOLD:     
                    case AA_BOLDFONT: fg_color = d->bold_color; break;
                    case AA_REVERSE:  fg_color = d->black_color; bg_color = d->normal_color; break;
                    case AA_SPECIAL:  fg_color = d->special_color; break;
                    default:          fg_color = d->normal_color; break;
                }
            }
            
            SDL_render_char(d, ch, x, y, fg_color, bg_color);
            
            d->previoust[pos] = ch;
            d->previousa[pos] = attr;
        }
    }
    
    if (d->cvisible) {
        SDL_SetRenderDrawColor(d->renderer, 255, 255, 255, 255);
        SDL_Rect cursor_rect = {
            d->Xpos * d->char_width,
            (d->Ypos + 1) * d->char_height - 2,
            d->char_width,
            2
        };
        SDL_RenderFillRect(d->renderer, &cursor_rect);
    }
    
    SDL_RenderPresent(d->renderer);
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
    SDL_print,
    SDL_gotoxy,
    SDL_flush,
    SDL_cursor
};

#endif
