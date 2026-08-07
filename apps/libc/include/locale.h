#ifndef ROBU_LIBC_LOCALE_H
#define ROBU_LIBC_LOCALE_H
#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5
#define LC_MESSAGES 6
struct lconv {
    char *decimal_point;
    char *thousands_sep;
};
char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);
typedef void *locale_t;
#define LC_CTYPE_MASK (1 << LC_CTYPE)
#define LC_ALL_MASK   (~0)
locale_t newlocale(int category_mask, const char *locale, locale_t base);
locale_t uselocale(locale_t newloc);
void freelocale(locale_t loc);
#endif
