#ifndef ROBU_LIBC_FNMATCH_H
#define ROBU_LIBC_FNMATCH_H
#define FNM_PATHNAME    (1 << 0)
#define FNM_CASEFOLD    (1 << 1)
#define FNM_LEADING_DIR (1 << 2)
#define FNM_NOMATCH     1
int fnmatch(const char *pattern, const char *string, int flags);
#endif
