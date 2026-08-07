#ifndef ROBU_LIBC_WCHAR_H
#define ROBU_LIBC_WCHAR_H
#include <sys/types.h>
#ifndef __WCHAR_TYPE_DEFINED
#define __WCHAR_TYPE_DEFINED
typedef int wchar_t;
#endif
typedef unsigned int wint_t;
#define WEOF ((wint_t)-1)
size_t wcslen(const wchar_t *s);
int wctomb(char *s, wchar_t wc);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);
int wcwidth(wchar_t c);
int mbrtowc(wchar_t *pwc, const char *s, size_t n, void *ps);
size_t mbrlen(const char *s, size_t n, void *ps);
size_t wcrtomb(char *s, wchar_t wc, void *ps);
#endif
