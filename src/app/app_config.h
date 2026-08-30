#ifndef HASCIICAM_APP_CONFIG_H
#define HASCIICAM_APP_CONFIG_H

#include <stddef.h>

typedef enum hasciicam_size_intent {
    HASCIICAM_SIZE_NONE = 0,
    HASCIICAM_SIZE_PIXELS,
    HASCIICAM_SIZE_CHARS
} hasciicam_size_intent;

typedef struct hasciicam_config {
    int quiet;
    int mode;
    int input_channel;
    int daemon_mode;
    int invert;
    int refresh;
    int aa_bright;
    int aa_contrast;
    int aa_gamma;
    int aa_dimmer;
    int fontsize;
    int linespace;
    int size_w;
    int size_h;
    hasciicam_size_intent size_intent;
    int explicit_size;
    int explicit_aadriver;
    int explicit_output;
    int sdl_vsync;
    int sdl_fullscreen;
    int mirror_x;
    int mirror_y;
    int virtual_camera;
    int virtual_camera_width;
    int virtual_camera_height;
    int virtual_camera_fps;
    int max_frames;
    int show_help;
    int show_aahelp;
    int show_version;
    int uid;
    int gid;
    char device[256];
    char image[512];
    char aafile[256];
    char background[64];
    char foreground[64];
    char font[64];
    char fontface[256];
    char aadriver[64];
    char sdl_renderer[32];
    char virtual_camera_device[256];
} hasciicam_config;

void hasciicam_config_init_defaults(hasciicam_config *cfg);
void hasciicam_config_parse(hasciicam_config *cfg,
                            int argc,
                            char *argv[],
                            const char *aa_help_text,
                            const char *package,
                            const char *version);
int hasciicam_config_load_env(hasciicam_config *cfg, char *err, size_t err_size);
int hasciicam_config_load_toml(hasciicam_config *cfg,
                               const char *path,
                               char *err,
                               size_t err_size);
int hasciicam_config_save_toml(const hasciicam_config *cfg,
                               const char *path,
                               char *err,
                               size_t err_size);

#endif
