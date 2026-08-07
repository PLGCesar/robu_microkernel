#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *optarg;
int optind = 1;
int opterr = 1;
int optopt;

static int cur_pos = 1;

int getopt(int argc, char *const argv[], const char *optstring) {
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
        return -1;
    }
    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }
    char c = argv[optind][cur_pos];
    const char *spec = strchr(optstring, c);
    if (!spec || c == ':') {
        optopt = c;
        if (opterr && optstring[0] != ':') {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
        }
        cur_pos++;
        if (argv[optind][cur_pos] == '\0') {
            optind++;
            cur_pos = 1;
        }
        return '?';
    }
    if (spec[1] == ':') {
        if (argv[optind][cur_pos + 1] != '\0') {
            optarg = &argv[optind][cur_pos + 1];
            optind++;
        } else if (optind + 1 < argc) {
            optarg = argv[optind + 1];
            optind += 2;
        } else {
            optopt = c;
            if (opterr && optstring[0] != ':') {
                fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
            }
            optind++;
            cur_pos = 1;
            return optstring[0] == ':' ? ':' : '?';
        }
        cur_pos = 1;
        return c;
    }
    cur_pos++;
    if (argv[optind][cur_pos] == '\0') {
        optind++;
        cur_pos = 1;
    }
    return c;
}
