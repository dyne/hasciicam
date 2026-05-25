#include "getopt.h"

#include <string.h>

char *optarg = 0;
int optind = 1;
int opterr = 1;
int optopt = 0;

static const char *find_short_option(const char *optstring, int option)
{
    while (*optstring) {
        if (*optstring == option)
            return optstring;
        optstring++;
    }
    return 0;
}

static int option_requires_argument(const char *short_option)
{
    return short_option && short_option[1] == ':';
}

int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex)
{
    char *arg;

    optarg = 0;
    if (optind >= argc)
        return -1;

    arg = argv[optind];
    if (!arg || arg[0] != '-' || arg[1] == '\0')
        return -1;

    if (strcmp(arg, "--") == 0) {
        optind++;
        return -1;
    }

    if (arg[1] == '-') {
        char *name = arg + 2;
        char *value = strchr(name, '=');
        size_t name_len = value ? (size_t)(value - name) : strlen(name);
        int i;

        for (i = 0; longopts && longopts[i].name; i++) {
            if (strlen(longopts[i].name) == name_len &&
                strncmp(longopts[i].name, name, name_len) == 0) {
                if (longindex)
                    *longindex = i;
                if (longopts[i].has_arg == required_argument) {
                    if (value) {
                        optarg = value + 1;
                    } else if (optind + 1 < argc) {
                        optarg = argv[++optind];
                    } else {
                        optopt = longopts[i].val;
                        optind++;
                        return '?';
                    }
                }
                optind++;
                if (longopts[i].flag) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
            }
        }

        optind++;
        return '?';
    }

    {
        int option = (unsigned char)arg[1];
        const char *short_option = find_short_option(optstring, option);

        optopt = option;
        if (!short_option) {
            optind++;
            return '?';
        }

        if (option_requires_argument(short_option)) {
            if (arg[2] != '\0') {
                optarg = arg + 2;
            } else if (optind + 1 < argc) {
                optarg = argv[++optind];
            } else {
                optind++;
                return '?';
            }
        }

        optind++;
        return option;
    }
}
