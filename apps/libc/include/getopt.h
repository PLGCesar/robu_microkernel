#ifndef ROBU_LIBC_GETOPT_H
#define ROBU_LIBC_GETOPT_H

extern char *optarg;
extern int optind, opterr, optopt;

int getopt(int argc, char *const argv[], const char *optstring);

#endif
