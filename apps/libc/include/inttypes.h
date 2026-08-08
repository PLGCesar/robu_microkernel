#ifndef ROBU_LIBC_INTTYPES_H
#define ROBU_LIBC_INTTYPES_H
#include <stdint.h>
intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);

#define PRId8  "hhd"
#define PRId16 "hd"
#define PRId32 "d"
#define PRId64 "ld"
#define PRIi8  "hhi"
#define PRIi16 "hi"
#define PRIi32 "i"
#define PRIi64 "li"
#define PRIu8  "hhu"
#define PRIu16 "hu"
#define PRIu32 "u"
#define PRIu64 "lu"
#define PRIx8  "hhx"
#define PRIx16 "hx"
#define PRIx32 "x"
#define PRIx64 "lx"
#define PRIo8  "hho"
#define PRIo16 "ho"
#define PRIo32 "o"
#define PRIo64 "lo"

#endif
