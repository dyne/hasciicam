#include "app_config.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "tomlc17/tomlc17.h"

#if defined(_WIN32)
#include "../compat/getopt.h"
#define strcasecmp _stricmp
#else
#include <getopt.h>
#include <unistd.h>
#endif

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

typedef enum config_value_kind {
    CONFIG_VALUE_BOOL = 0,
    CONFIG_VALUE_INT,
    CONFIG_VALUE_STRING,
    CONFIG_VALUE_MODE,
    CONFIG_VALUE_SIZE,
    CONFIG_VALUE_PIXEL_SIZE,
    CONFIG_VALUE_CHAR_SIZE,
    CONFIG_VALUE_SDL_VSYNC,
    CONFIG_VALUE_SDL_RENDERER,
    CONFIG_VALUE_MIRROR
} config_value_kind;

typedef struct config_key_desc {
    const char *key;
    config_value_kind kind;
    int persist;
    int allow_env_toml;
} config_key_desc;

static const struct option long_options[] = {
    {"frames", required_argument, NULL, 1000},
    {"pixel-size", required_argument, NULL, 1001},
    {"char-size", required_argument, NULL, 1002},
    {"sdl-renderer", required_argument, NULL, 1003},
    {"sdl-vsync", required_argument, NULL, 1004},
    {"fullscreen", no_argument, NULL, 1005},
    {"mirror", required_argument, NULL, 1006},
    {"config", required_argument, NULL, 1007},
    {"help", no_argument, NULL, 'h'},
    {"aahelp", no_argument, NULL, 'H'},
    {"version", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"mode", required_argument, NULL, 'm'},
    {"device", required_argument, NULL, 'd'},
    {"input", required_argument, NULL, 'i'},
    {"size", required_argument, NULL, 's'},
    {"aafile", required_argument, NULL, 'o'},
    {"aadriver", required_argument, NULL, 'O'},
    {"daemon", no_argument, NULL, 'D'},
    {"font-size", required_argument, NULL, 'S'},
    {"font-face", required_argument, NULL, 'a'},
    {"refresh", required_argument, NULL, 'r'},
    {"aabright", required_argument, NULL, 'b'},
    {"aacontrast", required_argument, NULL, 'c'},
    {"aagamma", required_argument, NULL, 'g'},
    {"invert", no_argument, NULL, 'I'},
    {"background", required_argument, NULL, 'B'},
    {"foreground", required_argument, NULL, 'F'},
    {"uid", required_argument, NULL, 'U'},
    {"gid", required_argument, NULL, 'G'},
    {0, 0, 0, 0}
};

static const char *short_options = "hHvqm:d:i:s:f:DS:a:r:o:b:c:g:IB:F:O:Q:U:G:";

static const char *help_text =
    "Usage: hasciicam [options] [rendering options] [aalib options]\n"
    "options:\n"
    " -h --help         this help\n"
    " -H --aahelp       aalib complete help\n"
    " -v --version      version information\n"
    " -q --quiet        be quiet\n"
    " -m --mode         mode: live|html|text      - default live\n"
    " -d --device       video grabbing device     - default /dev/video\n"
    " -i --input        input channel number      - default 1\n"
    " -s --size         contextual size WxH       - html chars, else window px\n"
    "    --pixel-size   output window size WxH    - preferred final live pixels\n"
    "    --char-size    output char size WxH      - preferred ascii grid\n"
    " -o --aafile       dumped file               - default hasciicam.[txt|html]\n"
    " -O --aadriver     aalib driver: X11|curses|SDL|stdout - default auto\n"
    "    --sdl-renderer accelerated|software|auto - SDL renderer choice\n"
    "    --sdl-vsync    on|off|auto               - SDL presentation sync\n"
    "    --fullscreen   start SDL live output fullscreen\n"
    "    --mirror       x|-x|y|-y                 - flip image, default x\n"
    "    --config       load startup TOML config  - default ./hasciicam.toml\n"
    " -D --daemon       run in background         - default foregrond\n"
    "    --frames N     stop after N rendered frames (test/smoke)\n"
    " -U --uid          setuid (int)              - default current\n"
    " -G --gid          setgid (int)              - default current\n"
    "rendering options:\n"
    " -S --font-size    html font size (1-4)      - default 1\n"
    " -a --font-face    html font to use          - default courier\n"
    " -r --refresh      refresh delay             - default 2\n"
    " -b --aabright     ascii brightness          - default 60\n"
    " -c --aacontrast   ascii contrast            - default 4\n"
    " -g --aagamma      ascii gamma               - default 3\n"
    " -I --invert       invert colors             - default off\n"
    " -B --background   background color (hex)    - default 000000\n"
    " -F --foreground   foreground color (hex)    - default 00FF00\n";

static const config_key_desc config_keys[] = {
    {"show_help", CONFIG_VALUE_BOOL, 0, 0},
    {"show_aahelp", CONFIG_VALUE_BOOL, 0, 0},
    {"show_version", CONFIG_VALUE_BOOL, 0, 0},
    {"quiet", CONFIG_VALUE_BOOL, 1, 1},
    {"mode", CONFIG_VALUE_MODE, 1, 1},
    {"device", CONFIG_VALUE_STRING, 1, 1},
    {"input", CONFIG_VALUE_INT, 1, 1},
    {"size", CONFIG_VALUE_SIZE, 0, 0},
    {"pixel_size", CONFIG_VALUE_PIXEL_SIZE, 1, 1},
    {"char_size", CONFIG_VALUE_CHAR_SIZE, 1, 1},
    {"output_file", CONFIG_VALUE_STRING, 1, 1},
    {"aa_driver", CONFIG_VALUE_STRING, 1, 1},
    {"daemon", CONFIG_VALUE_BOOL, 1, 1},
    {"font_size", CONFIG_VALUE_INT, 1, 1},
    {"font_face", CONFIG_VALUE_STRING, 1, 1},
    {"refresh", CONFIG_VALUE_INT, 1, 1},
    {"aa_bright", CONFIG_VALUE_INT, 1, 1},
    {"aa_contrast", CONFIG_VALUE_INT, 1, 1},
    {"aa_gamma", CONFIG_VALUE_INT, 1, 1},
    {"invert", CONFIG_VALUE_BOOL, 1, 1},
    {"background", CONFIG_VALUE_STRING, 1, 1},
    {"foreground", CONFIG_VALUE_STRING, 1, 1},
    {"uid", CONFIG_VALUE_INT, 1, 1},
    {"gid", CONFIG_VALUE_INT, 1, 1},
    {"frames", CONFIG_VALUE_INT, 1, 1},
    {"sdl_renderer", CONFIG_VALUE_SDL_RENDERER, 1, 1},
    {"sdl_vsync", CONFIG_VALUE_SDL_VSYNC, 1, 1},
    {"fullscreen", CONFIG_VALUE_BOOL, 1, 1},
    {"mirror", CONFIG_VALUE_MIRROR, 1, 1}
};

/* Parse WIDTHxHEIGHT where separator can be x or X. */
static int parse_wxh(const char *text, int *out_w, int *out_h) {
    char *end = NULL;
    long w = 0;
    long h = 0;
    if (text == NULL || out_w == NULL || out_h == NULL || text[0] == '\0')
        return 0;

    w = strtol(text, &end, 10);
    if (end == text || w <= 0)
        return 0;
    if (*end != 'x' && *end != 'X')
        return 0;
    h = strtol(end + 1, &end, 10);
    if (h <= 0)
        return 0;
    if (*end != '\0')
        return 0;

    *out_w = (int)w;
    *out_h = (int)h;
    return 1;
}

static int parse_sdl_vsync(const char *text, int *out_vsync) {
    if (text == NULL || out_vsync == NULL)
        return 0;
    if (strcasecmp(text, "on") == 0 || strcmp(text, "1") == 0 ||
        strcasecmp(text, "true") == 0 || strcasecmp(text, "yes") == 0) {
        *out_vsync = 1;
        return 1;
    }
    if (strcasecmp(text, "off") == 0 || strcmp(text, "0") == 0 ||
        strcasecmp(text, "false") == 0 || strcasecmp(text, "no") == 0) {
        *out_vsync = 0;
        return 1;
    }
    if (strcasecmp(text, "auto") == 0) {
        *out_vsync = -1;
        return 1;
    }
    return 0;
}

static int parse_sdl_renderer(const char *text, char *out_renderer, size_t out_size) {
    if (text == NULL || out_renderer == NULL || out_size == 0)
        return 0;
    if (strcasecmp(text, "accelerated") == 0 ||
        strcasecmp(text, "software") == 0 ||
        strcasecmp(text, "auto") == 0) {
        strncpy(out_renderer, text, out_size - 1);
        out_renderer[out_size - 1] = '\0';
        return 1;
    }
    return 0;
}

static int parse_mirror_axis(const char *text, int *mirror_x, int *mirror_y) {
    char token_buf[64];
    char *cursor = NULL;
    char *token = NULL;
    int seen = 0;

    if (text == NULL || mirror_x == NULL || mirror_y == NULL || text[0] == '\0')
        return 0;
    strncpy(token_buf, text, sizeof(token_buf) - 1);
    token_buf[sizeof(token_buf) - 1] = '\0';

    cursor = token_buf;
    token = strtok(cursor, ", \t");
    while (token != NULL) {
        if (strcmp(token, "x") == 0) {
            *mirror_x = 1;
            seen = 1;
        } else if (strcmp(token, "-x") == 0) {
            *mirror_x = 0;
            seen = 1;
        } else if (strcmp(token, "y") == 0) {
            *mirror_y = 1;
            seen = 1;
        } else if (strcmp(token, "-y") == 0) {
            *mirror_y = 0;
            seen = 1;
        } else {
            return 0;
        }
        token = strtok(NULL, ", \t");
    }
    return seen;
}

static void set_error(char *err, size_t err_size, const char *msg) {
    if (err == NULL || err_size == 0)
        return;
    if (msg == NULL)
        msg = "invalid configuration";
    strncpy(err, msg, err_size - 1);
    err[err_size - 1] = '\0';
}

static void set_errorf(char *err, size_t err_size, const char *fmt, const char *key) {
    if (err == NULL || err_size == 0)
        return;
    if (fmt == NULL)
        fmt = "invalid value";
#if defined(_WIN32)
    _snprintf(err, err_size - 1, fmt, key);
    err[err_size - 1] = '\0';
#else
    snprintf(err, err_size, fmt, key);
#endif
}

static int parse_bool_value(const char *value, int *out_value) {
    if (value == NULL || out_value == NULL)
        return 0;
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
        *out_value = 1;
        return 1;
    }
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int parse_int_value(const char *value, int *out_value) {
    char *end = NULL;
    long parsed = 0;
    if (value == NULL || out_value == NULL || value[0] == '\0')
        return 0;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0')
        return 0;
    if (errno == ERANGE || parsed > INT_MAX || parsed < INT_MIN)
        return 0;
    *out_value = (int)parsed;
    return 1;
}

static void apply_mode_side_effects(hasciicam_config *cfg) {
    if (cfg == NULL)
        return;
    if (!cfg->explicit_output) {
        if (cfg->mode == HTML) {
            strcpy(cfg->aafile, "hasciicam.html");
        } else if (cfg->mode == TEXT) {
            strcpy(cfg->aafile, "hasciicam.asc");
        }
    }
}

static int set_config_value(hasciicam_config *cfg,
                            const char *key,
                            const char *value,
                            char *err,
                            size_t err_size) {
    int iv = 0;
    int w = 0;
    int h = 0;

    if (cfg == NULL || key == NULL || value == NULL) {
        set_error(err, err_size, "invalid configuration input");
        return 0;
    }

    if (strcmp(key, "show_help") == 0) {
        if (!parse_bool_value(value, &cfg->show_help)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "show_aahelp") == 0) {
        if (!parse_bool_value(value, &cfg->show_aahelp)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "show_version") == 0) {
        if (!parse_bool_value(value, &cfg->show_version)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "quiet") == 0) {
        if (!parse_bool_value(value, &cfg->quiet)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "mode") == 0) {
        if (strcasecmp(value, "live") == 0) {
            cfg->mode = LIVE;
        } else if (strcasecmp(value, "html") == 0) {
            cfg->mode = HTML;
        } else if (strcasecmp(value, "text") == 0) {
            cfg->mode = TEXT;
        } else {
            set_error(err, err_size, "invalid value for mode: expected live, html, or text");
            return 0;
        }
        apply_mode_side_effects(cfg);
        return 1;
    }
    if (strcmp(key, "device") == 0) {
        strncpy(cfg->device, value, sizeof(cfg->device) - 1);
        cfg->device[sizeof(cfg->device) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "input") == 0) {
        if (!parse_int_value(value, &iv)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        if (iv > 3) {
            set_error(err, err_size, "invalid value for input: expected 0..3");
            return 0;
        }
        cfg->input_channel = iv;
        return 1;
    }
    if (strcmp(key, "size") == 0) {
        if (!parse_wxh(value, &w, &h)) {
            set_error(err, err_size, "invalid value for size: expected WxH");
            return 0;
        }
        cfg->size_w = w;
        cfg->size_h = h;
        cfg->size_intent = (cfg->mode == HTML) ? HASCIICAM_SIZE_CHARS : HASCIICAM_SIZE_PIXELS;
        cfg->explicit_size = 1;
        return 1;
    }
    if (strcmp(key, "pixel_size") == 0) {
        if (!parse_wxh(value, &w, &h)) {
            set_error(err, err_size, "invalid value for pixel_size: expected WxH");
            return 0;
        }
        cfg->size_w = w;
        cfg->size_h = h;
        cfg->size_intent = HASCIICAM_SIZE_PIXELS;
        cfg->explicit_size = 1;
        return 1;
    }
    if (strcmp(key, "char_size") == 0) {
        if (!parse_wxh(value, &w, &h)) {
            set_error(err, err_size, "invalid value for char_size: expected WxH");
            return 0;
        }
        cfg->size_w = w;
        cfg->size_h = h;
        cfg->size_intent = HASCIICAM_SIZE_CHARS;
        cfg->explicit_size = 1;
        return 1;
    }
    if (strcmp(key, "output_file") == 0) {
        strncpy(cfg->aafile, value, sizeof(cfg->aafile) - 1);
        cfg->aafile[sizeof(cfg->aafile) - 1] = '\0';
        cfg->explicit_output = 1;
        return 1;
    }
    if (strcmp(key, "aa_driver") == 0) {
        strncpy(cfg->aadriver, value, sizeof(cfg->aadriver) - 1);
        cfg->aadriver[sizeof(cfg->aadriver) - 1] = '\0';
        cfg->explicit_aadriver = 1;
        return 1;
    }
    if (strcmp(key, "daemon") == 0) {
        if (!parse_bool_value(value, &cfg->daemon_mode)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "font_size") == 0) {
        if (!parse_int_value(value, &iv)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        cfg->fontsize = iv;
        switch (cfg->fontsize) {
        case 1: cfg->linespace = 5; break;
        case 2: cfg->linespace = 10; break;
        case 3: cfg->linespace = 11; break;
        case 4: cfg->linespace = 13; break;
        default: cfg->linespace = 15; break;
        }
        return 1;
    }
    if (strcmp(key, "font_face") == 0) {
        strncpy(cfg->fontface, value, sizeof(cfg->fontface) - 1);
        cfg->fontface[sizeof(cfg->fontface) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "refresh") == 0) {
        if (!parse_int_value(value, &cfg->refresh)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "aa_bright") == 0) {
        if (!parse_int_value(value, &cfg->aa_bright)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "aa_contrast") == 0) {
        if (!parse_int_value(value, &cfg->aa_contrast)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "aa_gamma") == 0) {
        if (!parse_int_value(value, &cfg->aa_gamma)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "invert") == 0) {
        if (!parse_bool_value(value, &cfg->invert)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "background") == 0) {
        strncpy(cfg->background, value, sizeof(cfg->background) - 1);
        cfg->background[sizeof(cfg->background) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "foreground") == 0) {
        strncpy(cfg->foreground, value, sizeof(cfg->foreground) - 1);
        cfg->foreground[sizeof(cfg->foreground) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "uid") == 0) {
        if (!parse_int_value(value, &cfg->uid)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "gid") == 0) {
        if (!parse_int_value(value, &cfg->gid)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "frames") == 0) {
        if (!parse_int_value(value, &iv)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        cfg->max_frames = (iv < 0) ? 0 : iv;
        return 1;
    }
    if (strcmp(key, "sdl_renderer") == 0) {
        if (!parse_sdl_renderer(value, cfg->sdl_renderer, sizeof(cfg->sdl_renderer))) {
            set_error(err, err_size, "invalid value for sdl_renderer: expected accelerated, software, or auto");
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "sdl_vsync") == 0) {
        if (!parse_sdl_vsync(value, &cfg->sdl_vsync)) {
            set_error(err, err_size, "invalid value for sdl_vsync: expected on, off, or auto");
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "fullscreen") == 0) {
        if (!parse_bool_value(value, &cfg->sdl_fullscreen)) {
            set_errorf(err, err_size, "invalid value for %s", key);
            return 0;
        }
        return 1;
    }
    if (strcmp(key, "mirror") == 0) {
        if (!parse_mirror_axis(value, &cfg->mirror_x, &cfg->mirror_y)) {
            set_error(err, err_size, "invalid value for mirror: expected x/-x/y/-y tokens");
            return 0;
        }
        return 1;
    }

    set_errorf(err, err_size, "unknown config key %s", key);
    return 0;
}

static int config_key_is_persisted(const char *key) {
    size_t i = 0;
    for (i = 0; i < sizeof(config_keys) / sizeof(config_keys[0]); ++i) {
        if (strcmp(config_keys[i].key, key) == 0)
            return config_keys[i].persist;
    }
    return 0;
}

static int config_value_as_string(const hasciicam_config *cfg,
                                  const char *key,
                                  char *out,
                                  size_t out_size,
                                  int *out_is_quoted) {
    if (cfg == NULL || key == NULL || out == NULL || out_size == 0 || out_is_quoted == NULL)
        return 0;

    if (strcmp(key, "quiet") == 0) {
        snprintf(out, out_size, "%s", cfg->quiet ? "true" : "false");
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "mode") == 0) {
        const char *mode = "live";
        if (cfg->mode == HTML)
            mode = "html";
        else if (cfg->mode == TEXT)
            mode = "text";
        snprintf(out, out_size, "%s", mode);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "device") == 0) {
        snprintf(out, out_size, "%s", cfg->device);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "input") == 0) {
        snprintf(out, out_size, "%d", cfg->input_channel);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "pixel_size") == 0 || strcmp(key, "char_size") == 0) {
        if (!cfg->explicit_size)
            return 0;
        if ((strcmp(key, "pixel_size") == 0 && cfg->size_intent != HASCIICAM_SIZE_PIXELS) ||
            (strcmp(key, "char_size") == 0 && cfg->size_intent != HASCIICAM_SIZE_CHARS)) {
            return 0;
        }
        snprintf(out, out_size, "%dx%d", cfg->size_w, cfg->size_h);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "output_file") == 0) {
        snprintf(out, out_size, "%s", cfg->aafile);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "aa_driver") == 0) {
        snprintf(out, out_size, "%s", cfg->aadriver);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "daemon") == 0) {
        snprintf(out, out_size, "%s", cfg->daemon_mode ? "true" : "false");
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "font_size") == 0) {
        snprintf(out, out_size, "%d", cfg->fontsize);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "font_face") == 0) {
        snprintf(out, out_size, "%s", cfg->fontface);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "refresh") == 0) {
        snprintf(out, out_size, "%d", cfg->refresh);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "aa_bright") == 0) {
        snprintf(out, out_size, "%d", cfg->aa_bright);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "aa_contrast") == 0) {
        snprintf(out, out_size, "%d", cfg->aa_contrast);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "aa_gamma") == 0) {
        snprintf(out, out_size, "%d", cfg->aa_gamma);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "invert") == 0) {
        snprintf(out, out_size, "%s", cfg->invert ? "true" : "false");
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "background") == 0) {
        snprintf(out, out_size, "%s", cfg->background);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "foreground") == 0) {
        snprintf(out, out_size, "%s", cfg->foreground);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "uid") == 0) {
        snprintf(out, out_size, "%d", cfg->uid);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "gid") == 0) {
        snprintf(out, out_size, "%d", cfg->gid);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "frames") == 0) {
        snprintf(out, out_size, "%d", cfg->max_frames);
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "sdl_renderer") == 0) {
        if (cfg->sdl_renderer[0] == '\0')
            return 0;
        snprintf(out, out_size, "%s", cfg->sdl_renderer);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "sdl_vsync") == 0) {
        const char *vsync = NULL;
        if (cfg->sdl_vsync == 1)
            vsync = "on";
        else if (cfg->sdl_vsync == 0)
            vsync = "off";
        else if (cfg->sdl_vsync == -1)
            vsync = "auto";
        else
            return 0;
        snprintf(out, out_size, "%s", vsync);
        *out_is_quoted = 1;
        return 1;
    }
    if (strcmp(key, "fullscreen") == 0) {
        snprintf(out, out_size, "%s", cfg->sdl_fullscreen ? "true" : "false");
        *out_is_quoted = 0;
        return 1;
    }
    if (strcmp(key, "mirror") == 0) {
        snprintf(out, out_size, "%s,%s",
                 cfg->mirror_x ? "x" : "-x",
                 cfg->mirror_y ? "y" : "-y");
        *out_is_quoted = 1;
        return 1;
    }

    return 0;
}

static void toml_escape_and_write(FILE *fp, const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    fputc('"', fp);
    while (*p != '\0') {
        unsigned char ch = *p++;
        if (ch == '\\') {
            fputs("\\\\", fp);
        } else if (ch == '"') {
            fputs("\\\"", fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch < 0x20) {
            fprintf(fp, "\\u%04x", (unsigned int)ch);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

void hasciicam_config_init_defaults(hasciicam_config *cfg) {
    if (cfg == NULL)
        return;
    memset(cfg, 0, sizeof(*cfg));

#if defined(_WIN32)
    cfg->device[0] = '\0';
#else
    {
        struct stat st;
        if (stat("/dev/video", &st) < 0)
            strcpy(cfg->device, "/dev/video0");
        else
            strcpy(cfg->device, "/dev/video");
    }
#endif

    strcpy(cfg->background, "000000");
    strcpy(cfg->foreground, "00FF00");
    strcpy(cfg->fontface, "courier");
    strcpy(cfg->aadriver, "");
    strcpy(cfg->sdl_renderer, "");
    strcpy(cfg->aafile, "");

    cfg->mode = LIVE;
    cfg->input_channel = 0;
    cfg->refresh = 2;
    cfg->aa_bright = 60;
    cfg->aa_contrast = 4;
    cfg->aa_gamma = 3;
    cfg->fontsize = 1;
    cfg->linespace = 5;
    cfg->uid = -1;
    cfg->gid = -1;
    cfg->max_frames = 0;
    cfg->explicit_size = 0;
    cfg->explicit_aadriver = 0;
    cfg->explicit_output = 0;
    cfg->sdl_vsync = -2;
    cfg->sdl_fullscreen = 0;
    cfg->mirror_x = 1;
    cfg->mirror_y = 0;
}

int hasciicam_config_load_env(hasciicam_config *cfg, char *err, size_t err_size) {
    size_t i = 0;
    if (cfg == NULL) {
        set_error(err, err_size, "null config");
        return 0;
    }
    for (i = 0; i < sizeof(config_keys) / sizeof(config_keys[0]); ++i) {
        if (!config_keys[i].allow_env_toml)
            continue;
        const char *value = getenv(config_keys[i].key);
        if (value == NULL || value[0] == '\0')
            continue;
        if (!set_config_value(cfg, config_keys[i].key, value, err, err_size))
            return 0;
    }
    return 1;
}

int hasciicam_config_load_toml(hasciicam_config *cfg,
                               const char *path,
                               char *err,
                               size_t err_size) {
    toml_result_t result;
    int ok = 1;
    int i = 0;
    char value_buf[128];
    if (cfg == NULL || path == NULL) {
        set_error(err, err_size, "null config or path");
        return 0;
    }

    result = toml_parse_file_ex(path);
    if (!result.ok) {
        set_error(err, err_size, result.errmsg);
        toml_free(result);
        return 0;
    }

    if (result.toptab.type != TOML_TABLE) {
        set_error(err, err_size, "toml root must be a table");
        toml_free(result);
        return 0;
    }

    for (i = 0; i < result.toptab.u.tab.size; ++i) {
        const char *key = result.toptab.u.tab.key[i];
        toml_datum_t value = result.toptab.u.tab.value[i];
        int has_known_key = 0;
        size_t k = 0;
        for (k = 0; k < sizeof(config_keys) / sizeof(config_keys[0]); ++k) {
            if (strcmp(config_keys[k].key, key) == 0) {
                if (!config_keys[k].allow_env_toml) {
                    set_errorf(err, err_size, "key not allowed in env/toml: %s", key);
                    ok = 0;
                    break;
                }
                has_known_key = 1;
                break;
            }
        }
        if (!ok)
            break;
        if (!has_known_key) {
            set_errorf(err, err_size, "unknown config key %s", key);
            ok = 0;
            break;
        }

        if (value.type == TOML_STRING) {
            if (!set_config_value(cfg, key, value.u.s, err, err_size)) {
                ok = 0;
                break;
            }
        } else if (value.type == TOML_INT64) {
            snprintf(value_buf, sizeof(value_buf), "%lld", (long long)value.u.int64);
            if (!set_config_value(cfg, key, value_buf, err, err_size)) {
                ok = 0;
                break;
            }
        } else if (value.type == TOML_BOOLEAN) {
            if (!set_config_value(cfg, key, value.u.boolean ? "true" : "false", err, err_size)) {
                ok = 0;
                break;
            }
        } else {
            set_errorf(err, err_size, "unsupported TOML type for key %s", key);
            ok = 0;
            break;
        }
    }

    toml_free(result);
    return ok;
}

static int file_exists(const char *path) {
    FILE *fp = NULL;
    if (path == NULL || path[0] == '\0')
        return 0;
    fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

static const char *find_explicit_config_path(int argc, char *argv[]) {
    int i = 1;
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg == NULL)
            continue;
        if (strcmp(arg, "--config") == 0) {
            if (i + 1 >= argc)
                return "";
            return argv[i + 1];
        }
        if (strncmp(arg, "--config=", 9) == 0)
            return arg + 9;
    }
    return NULL;
}

static void load_startup_config(hasciicam_config *cfg, int argc, char *argv[]) {
    char err[200];
    const char *path = find_explicit_config_path(argc, argv);
    int explicit_path = path != NULL;

    if (!explicit_path)
        path = "hasciicam.toml";

    if (path[0] == '\0') {
        fprintf(stderr, "!! --config requires a path\n");
        exit(1);
    }
    if (!explicit_path && !file_exists(path))
        return;
    if (!hasciicam_config_load_toml(cfg, path, err, sizeof(err))) {
        fprintf(stderr, "!! cannot load config %s: %s\n", path, err);
        exit(1);
    }
}

int hasciicam_config_save_toml(const hasciicam_config *cfg,
                               const char *path,
                               char *err,
                               size_t err_size) {
    FILE *fp = NULL;
    size_t i = 0;
    if (cfg == NULL || path == NULL) {
        set_error(err, err_size, "null config or path");
        return 0;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        set_error(err, err_size, "cannot open file for writing");
        return 0;
    }

    for (i = 0; i < sizeof(config_keys) / sizeof(config_keys[0]); ++i) {
        char value[256];
        int quoted = 0;
        if (!config_key_is_persisted(config_keys[i].key))
            continue;
        if (!config_value_as_string(cfg, config_keys[i].key, value, sizeof(value), &quoted))
            continue;
        fprintf(fp, "%s = ", config_keys[i].key);
        if (quoted)
            toml_escape_and_write(fp, value);
        else
            fputs(value, fp);
        fputc('\n', fp);
    }

    if (fclose(fp) != 0) {
        set_error(err, err_size, "failed to close config file");
        return 0;
    }
    return 1;
}

void hasciicam_config_parse(hasciicam_config *cfg,
                            int argc,
                            char *argv[],
                            const char *aa_help_text,
                            const char *package,
                            const char *version) {
    int res = 0;
    int short_size_w = 0;
    int short_size_h = 0;
    int short_size_set = 0;
    char env_err[200];

    if (cfg == NULL)
        return;

    load_startup_config(cfg, argc, argv);

    if (!hasciicam_config_load_env(cfg, env_err, sizeof(env_err))) {
        fprintf(stderr, "!! %s\n", env_err);
        exit(1);
    }

    do {
        res = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (res) {
        case 1000:
            if (!set_config_value(cfg, "frames", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'h':
            cfg->show_help = 1;
            break;
        case 'H':
            cfg->show_aahelp = 1;
            break;
        case 'v':
            cfg->show_version = 1;
            break;
        case 'q':
            cfg->quiet = 1;
            break;
        case 'm':
            if (!set_config_value(cfg, "mode", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'd':
            if (!set_config_value(cfg, "device", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'i':
            if (!set_config_value(cfg, "input", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 's':
            if (!parse_wxh(optarg, &short_size_w, &short_size_h)) {
                fprintf(stderr, "!! invalid size '%s', expected WxH\n", optarg);
                exit(1);
            }
            short_size_set = 1;
            cfg->explicit_size = 1;
            break;
        case 1001:
            if (!set_config_value(cfg, "pixel_size", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 1002:
            if (!set_config_value(cfg, "char_size", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 1003:
            if (!set_config_value(cfg, "sdl_renderer", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 1004:
            if (!set_config_value(cfg, "sdl_vsync", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 1005:
            cfg->sdl_fullscreen = 1;
            break;
        case 1006:
            if (!set_config_value(cfg, "mirror", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 1007:
            break;
        case 'S':
            if (!set_config_value(cfg, "font_size", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'a':
            if (!set_config_value(cfg, "font_face", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'r':
            if (!set_config_value(cfg, "refresh", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'o':
            if (cfg->mode > 0) {
                if (!set_config_value(cfg, "output_file", optarg, env_err, sizeof(env_err))) {
                    fprintf(stderr, "!! %s\n", env_err);
                    exit(1);
                }
            }
            break;
        case 'O':
            if (!set_config_value(cfg, "aa_driver", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'D':
            cfg->daemon_mode = 1;
            break;
        case 'b':
            if (!set_config_value(cfg, "aa_bright", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'c':
            if (!set_config_value(cfg, "aa_contrast", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'g':
            if (!set_config_value(cfg, "aa_gamma", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'I':
            cfg->invert = 1;
            break;
        case 'B':
            if (!set_config_value(cfg, "background", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'F':
            if (!set_config_value(cfg, "foreground", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'U':
            if (!set_config_value(cfg, "uid", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        case 'G':
            if (!set_config_value(cfg, "gid", optarg, env_err, sizeof(env_err))) {
                fprintf(stderr, "!! %s\n", env_err);
                exit(1);
            }
            break;
        default:
            break;
        }
    } while (res > 0);

    if (short_size_set) {
        cfg->size_w = short_size_w;
        cfg->size_h = short_size_h;
        if (cfg->mode == HTML)
            cfg->size_intent = HASCIICAM_SIZE_CHARS;
        else
            cfg->size_intent = HASCIICAM_SIZE_PIXELS;
    }

    if (cfg->show_help) {
        fprintf(stderr, "%s", help_text);
        exit(0);
    }
    if (cfg->show_aahelp) {
        fprintf(stderr, "%s", help_text);
        fprintf(stderr, "\naalib options:\n%s", aa_help_text);
        exit(0);
    }
    if (cfg->show_version) {
        fprintf(stderr,
                "\n%s %s - (h)ascii 4 the masses! - https://ascii.dyne.org\n"
                "(c)2000-2025 RASTASOFT by Jaromil @ Dyne.org\n\n",
                package, version);
        exit(0);
    }

    if (cfg->mode == HTML && cfg->size_intent == HASCIICAM_SIZE_PIXELS) {
        fprintf(stderr, "!! html mode does not accept pixel size; use --char-size WxH or -s WxH\n");
        exit(1);
    }
}
