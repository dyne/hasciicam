#include "app_config.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif

/* hasciicam modes */
#define LIVE 0
#define HTML 1
#define TEXT 2

static const struct option long_options[] = {
    {"frames", required_argument, NULL, 1000},
    {"pixel-size", required_argument, NULL, 1001},
    {"char-size", required_argument, NULL, 1002},
    {"sdl-renderer", required_argument, NULL, 1003},
    {"sdl-vsync", required_argument, NULL, 1004},
    {"fullscreen", no_argument, NULL, 1005},
    {"mirror", required_argument, NULL, 1006},
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
    if (text == NULL || mirror_x == NULL || mirror_y == NULL)
        return 0;
    if (strcmp(text, "x") == 0) {
        *mirror_x = 1;
        return 1;
    }
    if (strcmp(text, "-x") == 0) {
        *mirror_x = 0;
        return 1;
    }
    if (strcmp(text, "y") == 0) {
        *mirror_y = 1;
        return 1;
    }
    if (strcmp(text, "-y") == 0) {
        *mirror_y = 0;
        return 1;
    }
    return 0;
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
    cfg->sdl_vsync = -2;
    cfg->sdl_fullscreen = 0;
    cfg->mirror_x = 1;
    cfg->mirror_y = 0;
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

    if (cfg == NULL)
        return;

    do {
        res = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (res) {
        case 1000:
            cfg->max_frames = atoi(optarg);
            if (cfg->max_frames < 0)
                cfg->max_frames = 0;
            break;
        case 'h':
            fprintf(stderr, "%s", help_text);
            exit(0);
            break;
        case 'H':
            fprintf(stderr, "%s", help_text);
            fprintf(stderr, "\naalib options:\n%s", aa_help_text);
            exit(0);
            break;
        case 'v':
            fprintf(stderr,
                    "\n%s %s - (h)ascii 4 the masses! - https://ascii.dyne.org\n"
                    "(c)2000-2025 RASTASOFT by Jaromil @ Dyne.org\n\n",
                    package, version);
            exit(0);
            break;
        case 'q':
            cfg->quiet = 1;
            break;
        case 'm':
            if (strcasecmp(optarg, "live") == 0) {
                cfg->mode = LIVE;
            } else if (strcasecmp(optarg, "html") == 0) {
                cfg->mode = HTML;
                strcpy(cfg->aafile, "hasciicam.html");
            } else if (strcasecmp(optarg, "text") == 0) {
                cfg->mode = TEXT;
                strcpy(cfg->aafile, "hasciicam.asc");
            } else {
                fprintf(stderr, "!! invalid mode selected, using live\n");
                cfg->mode = LIVE;
            }
            break;
        case 'd':
            strncpy(cfg->device, optarg, sizeof(cfg->device) - 1);
            cfg->device[sizeof(cfg->device) - 1] = '\0';
            break;
        case 'i':
            cfg->input_channel = atoi(optarg);
            if (cfg->input_channel > 3) {
                fprintf(stderr, "invalid input selected\n");
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
            if (!parse_wxh(optarg, &cfg->size_w, &cfg->size_h)) {
                fprintf(stderr, "!! invalid pixel size '%s', expected WxH\n", optarg);
                exit(1);
            }
            cfg->size_intent = HASCIICAM_SIZE_PIXELS;
            cfg->explicit_size = 1;
            break;
        case 1002:
            if (!parse_wxh(optarg, &cfg->size_w, &cfg->size_h)) {
                fprintf(stderr, "!! invalid char size '%s', expected WxH\n", optarg);
                exit(1);
            }
            cfg->size_intent = HASCIICAM_SIZE_CHARS;
            cfg->explicit_size = 1;
            break;
        case 1003:
            if (!parse_sdl_renderer(optarg, cfg->sdl_renderer, sizeof(cfg->sdl_renderer))) {
                fprintf(stderr, "!! invalid SDL renderer '%s', expected accelerated, software, or auto\n", optarg);
                exit(1);
            }
            break;
        case 1004:
            if (!parse_sdl_vsync(optarg, &cfg->sdl_vsync)) {
                fprintf(stderr, "!! invalid SDL vsync '%s', expected on, off, or auto\n", optarg);
                exit(1);
            }
            break;
        case 1005:
            cfg->sdl_fullscreen = 1;
            break;
        case 1006:
            if (!parse_mirror_axis(optarg, &cfg->mirror_x, &cfg->mirror_y)) {
                fprintf(stderr, "!! invalid mirror axis '%s', expected x, -x, y, or -y\n", optarg);
                exit(1);
            }
            break;
        case 'S':
            cfg->fontsize = atoi(optarg);
            switch (cfg->fontsize) {
            case 1: cfg->linespace = 5; break;
            case 2: cfg->linespace = 10; break;
            case 3: cfg->linespace = 11; break;
            case 4: cfg->linespace = 13; break;
            default: cfg->linespace = 15; break;
            }
            break;
        case 'a':
            strncpy(cfg->fontface, optarg, sizeof(cfg->fontface) - 1);
            cfg->fontface[sizeof(cfg->fontface) - 1] = '\0';
            break;
        case 'r':
            cfg->refresh = atoi(optarg);
            break;
        case 'o':
            if (cfg->mode > 0) {
                strncpy(cfg->aafile, optarg, sizeof(cfg->aafile) - 1);
                cfg->aafile[sizeof(cfg->aafile) - 1] = '\0';
            }
            break;
        case 'O':
            strncpy(cfg->aadriver, optarg, sizeof(cfg->aadriver) - 1);
            cfg->aadriver[sizeof(cfg->aadriver) - 1] = '\0';
            cfg->explicit_aadriver = 1;
            break;
        case 'D':
            cfg->daemon_mode = 1;
            break;
        case 'b':
            cfg->aa_bright = atoi(optarg);
            break;
        case 'c':
            cfg->aa_contrast = atoi(optarg);
            break;
        case 'g':
            cfg->aa_gamma = atoi(optarg);
            break;
        case 'I':
            cfg->invert = 1;
            break;
        case 'B':
            strncpy(cfg->background, optarg, sizeof(cfg->background) - 1);
            cfg->background[sizeof(cfg->background) - 1] = '\0';
            break;
        case 'F':
            strncpy(cfg->foreground, optarg, sizeof(cfg->foreground) - 1);
            cfg->foreground[sizeof(cfg->foreground) - 1] = '\0';
            break;
        case 'U':
            cfg->uid = atoi(optarg);
            break;
        case 'G':
            cfg->gid = atoi(optarg);
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

    if (cfg->mode == HTML && cfg->size_intent == HASCIICAM_SIZE_PIXELS) {
        fprintf(stderr, "!! html mode does not accept pixel size; use --char-size WxH or -s WxH\n");
        exit(1);
    }
}
