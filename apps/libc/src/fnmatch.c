#include <fnmatch.h>
#include <string.h>
#include <ctype.h>
static int do_match(const char *pat, const char *str, int flags) {
    while (*pat) {
        if (*pat == '*') {
            do {
                pat++;
            } while (*pat == '*');
            if (!*pat) {
                if (flags & FNM_PATHNAME) {
                    return strchr(str, '/') ? FNM_NOMATCH : 0;
                }
                return 0;
            }
            for (; *str; str++) {
                if ((flags & FNM_PATHNAME) && *str == '/') {
                    break;
                }
                if (do_match(pat, str, flags) == 0) {
                    return 0;
                }
            }
            return do_match(pat, str, flags);
        }
        if (!*str) {
            return FNM_NOMATCH;
        }
        if (*pat == '?') {
            if ((flags & FNM_PATHNAME) && *str == '/') {
                return FNM_NOMATCH;
            }
            pat++;
            str++;
            continue;
        }
        if (*pat == '[') {
            if ((flags & FNM_PATHNAME) && *str == '/') {
                return FNM_NOMATCH;
            }
            pat++;
            int neg = 0;
            if (*pat == '!' || *pat == '^') {
                neg = 1;
                pat++;
            }
            int matched = 0;
            int first = 1;
            int c = (unsigned char)*str;
            if (flags & FNM_CASEFOLD) {
                c = tolower(c);
            }
            while (*pat && (*pat != ']' || first)) {
                first = 0;
                int lo = (unsigned char)*pat++;
                if (flags & FNM_CASEFOLD) {
                    lo = tolower(lo);
                }
                if (*pat == '-' && pat[1] && pat[1] != ']') {
                    pat++;
                    int hi = (unsigned char)*pat++;
                    if (flags & FNM_CASEFOLD) {
                        hi = tolower(hi);
                    }
                    if (c >= lo && c <= hi) {
                        matched = 1;
                    }
                } else if (c == lo) {
                    matched = 1;
                }
            }
            if (*pat == ']') {
                pat++;
            }
            if (neg) {
                matched = !matched;
            }
            if (!matched) {
                return FNM_NOMATCH;
            }
            str++;
            continue;
        }
        int pc = (unsigned char)*pat, sc = (unsigned char)*str;
        if (flags & FNM_CASEFOLD) {
            pc = tolower(pc);
            sc = tolower(sc);
        }
        if (pc != sc) {
            return FNM_NOMATCH;
        }
        pat++;
        str++;
    }
    return *str ? FNM_NOMATCH : 0;
}
int fnmatch(const char *pattern, const char *string, int flags) {
    return do_match(pattern, string, flags);
}
