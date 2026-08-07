#include <sys/stat.h>
#include <locale.h>
#include <langinfo.h>
static mode_t current_umask = 022;
mode_t umask(mode_t mask) {
    mode_t old = current_umask;
    current_umask = mask & 0777;
    return old;
}
char *setlocale(int category, const char *locale) {
    (void)category;
    (void)locale;
    return (char *)"C";
}
struct lconv *localeconv(void) {
    static struct lconv lc = { .decimal_point = (char *)".", .thousands_sep = (char *)"" };
    return &lc;
}
char *nl_langinfo(int item) {
    (void)item;
    return (char *)"UTF-8";
}
locale_t newlocale(int category_mask, const char *locale, locale_t base) {
    (void)category_mask;
    (void)locale;
    (void)base;
    return (locale_t)0;
}
locale_t uselocale(locale_t newloc) {
    (void)newloc;
    return (locale_t)0;
}
void freelocale(locale_t loc) {
    (void)loc;
}
