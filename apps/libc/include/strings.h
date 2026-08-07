#ifndef ROBU_LIBC_STRINGS_H
#define ROBU_LIBC_STRINGS_H
#include <sys/types.h>
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
int ffs(int i);
int bcmp(const void *a, const void *b, size_t n);
void bzero(void *s, size_t n);
#endif
