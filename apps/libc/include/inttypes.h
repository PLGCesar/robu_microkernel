#ifndef ROBU_LIBC_INTTYPES_H
#define ROBU_LIBC_INTTYPES_H
#include <stdint.h>
intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);
#endif
