#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "robu/types.h"
static unsigned long long strtoull_core(const char *s, char **endptr, int base, int *neg) {
    while (isspace((unsigned char)*s)) {
        s++;
    }
    *neg = 0;
    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        *neg = 1;
        s++;
    }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0 && s[0] == '0') {
        base = 8;
    } else if (base == 0) {
        base = 10;
    }
    unsigned long long acc = 0;
    const char *start = s;
    for (;; s++) {
        int c = (unsigned char)*s;
        int digit;
        if (isdigit(c)) {
            digit = c - '0';
        } else if (isalpha(c)) {
            digit = tolower(c) - 'a' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        acc = acc * (unsigned)base + (unsigned)digit;
    }
    if (endptr) {
        *endptr = (char *)(s == start ? start : s);
    }
    return acc;
}
unsigned long long strtoull(const char *s, char **endptr, int base) {
    int neg;
    unsigned long long v = strtoull_core(s, endptr, base, &neg);
    return neg ? (unsigned long long)(-(long long)v) : v;
}
long long strtoll(const char *s, char **endptr, int base) {
    int neg;
    unsigned long long v = strtoull_core(s, endptr, base, &neg);
    return neg ? -(long long)v : (long long)v;
}
unsigned long strtoul(const char *s, char **endptr, int base) {
    return (unsigned long)strtoull(s, endptr, base);
}
long strtol(const char *s, char **endptr, int base) {
    return (long)strtoll(s, endptr, base);
}
__attribute__((target("sse,sse2")))
double strtod(const char *s, char **endptr) {
    if (endptr) {
        *endptr = (char *)s;
    }
    errno = ENOSYS;
    return 0.0;
}
long double strtold(const char *s, char **endptr) {
    if (endptr) {
        *endptr = (char *)s;
    }
    errno = ENOSYS;
    return 0.0L;
}
int mkstemp(char *template_str) {
    (void)template_str;
    errno = ENOSYS;
    return -1;
}
int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
long long atoll(const char *s) { return strtoll(s, 0, 10); }
int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
static void swap_bytes(char *a, char *b, size_t size) {
    while (size--) {
        char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}
static void qsort_range(char *arr, long lo, long hi, size_t size,
                        int (*compar)(const void *, const void *)) {
    while (lo < hi) {
        char *pivot = arr + hi * (long)size;
        long i = lo - 1;
        for (long j = lo; j < hi; j++) {
            if (compar(arr + j * (long)size, pivot) < 0) {
                i++;
                swap_bytes(arr + i * (long)size, arr + j * (long)size, size);
            }
        }
        swap_bytes(arr + (i + 1) * (long)size, arr + hi * (long)size, size);
        long p = i + 1;
        if (p - lo < hi - p) {
            qsort_range(arr, lo, p - 1, size, compar);
            lo = p + 1;
        } else {
            qsort_range(arr, p + 1, hi, size, compar);
            hi = p - 1;
        }
    }
}
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *)) {
    if (nmemb < 2) {
        return;
    }
    qsort_range((char *)base, 0, (long)nmemb - 1, size, compar);
}
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *)) {
    const char *arr = base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const char *elem = arr + mid * size;
        int c = compar(key, elem);
        if (c == 0) {
            return (void *)elem;
        }
        if (c < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return 0;
}
static uint64_t rng_state = 0x2545F4914F6CDD1DULL;
void srandom(unsigned int seed) {
    rng_state = seed ? seed : 1;
}
long random(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (long)(rng_state & 0x7fffffff);
}
