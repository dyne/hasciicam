#ifndef __AASDLINT_H__
#define __AASDLINT_H__

#include <SDL2/SDL.h>

struct sdldriverdata {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *font_texture;
    SDL_Texture *stream_texture;
    SDL_PixelFormat *stream_format;
    Uint32 *stream_pixels;
    
    int width;
    int height;
    int stream_width;
    int stream_height;
    int char_width;
    int char_height;
    int x_offset_px;
    int y_offset_px;
    int force_clear;
    
    int black_color;
    int dim_color;
    int normal_color;
    int bold_color;
    int special_color;
    
    int current_attr;
    int inverted;
    int cvisible;
    int fullscreen;
    int Xpos;
    int Ypos;
    
    unsigned char *previoust;
    unsigned char *previousa;
    
    __AA_CONST struct aa_font *font;
};

#endif
