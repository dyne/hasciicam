#include "app_config.h"

#include <ctype.h>
#include <errno.h>
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
    " -s --size         ascii image size WxH      - webcam's smallest default\n"
    " -o --aafile       dumped file               - default hasciicam.[txt|html]\n"
    " -O --aadriver     aalib driver: X11|curses|SDL|stdout - default auto\n"
    " -D --daemon       run in background         - default foregrond\n"
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
}

void hasciicam_config_parse(hasciicam_config *cfg,
                            int argc,
                            char *argv[],
                            const char *aa_help_text,
                            const char *package,
                            const char *version) {
    int res = 0;

    if (cfg == NULL)
        return;

    do {
        res = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (res) {
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
        case 's': {
            char *t = optarg;
            char *tt;
            while (isdigit(*t))
                t++;
            *t = 0;
            cfg->user_w = atoi(optarg);
            tt = ++t;
            while (isdigit(*tt))
                tt++;
            *tt = 0;
            cfg->user_h = atoi(t);
            cfg->whchanged = 1;
            break;
        }
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
}
