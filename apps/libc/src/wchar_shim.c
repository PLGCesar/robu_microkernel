#include <wchar.h>
#include <string.h>
#include <errno.h>
size_t wcslen(const wchar_t *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}
int mbrtowc(wchar_t *pwc, const char *s, size_t n, void *ps) {
    (void)ps;
    if (n == 0) {
        return -2;
    }
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) {
        if (pwc) {
            *pwc = c;
        }
        return c ? 1 : 0;
    }
    int extra;
    wchar_t wc;
    if ((c & 0xE0) == 0xC0) {
        extra = 1;
        wc = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extra = 2;
        wc = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extra = 3;
        wc = c & 0x07;
    } else {
        errno = EILSEQ;
        return -1;
    }
    if (n < (size_t)(extra + 1)) {
        return -2;
    }
    for (int i = 1; i <= extra; i++) {
        unsigned char cc = (unsigned char)s[i];
        if ((cc & 0xC0) != 0x80) {
            errno = EILSEQ;
            return -1;
        }
        wc = (wchar_t)((wc << 6) | (cc & 0x3F));
    }
    if (pwc) {
        *pwc = wc;
    }
    return extra + 1;
}
size_t wcrtomb(char *s, wchar_t wc, void *ps) {
    (void)ps;
    if (!s) {
        return 1;
    }
    unsigned char *out = (unsigned char *)s;
    unsigned int uc = (unsigned int)wc;
    if (uc < 0x80) {
        out[0] = (unsigned char)uc;
        return 1;
    }
    if (uc < 0x800) {
        out[0] = (unsigned char)(0xC0 | (uc >> 6));
        out[1] = (unsigned char)(0x80 | (uc & 0x3F));
        return 2;
    }
    if (uc < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (uc >> 12));
        out[1] = (unsigned char)(0x80 | ((uc >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (uc & 0x3F));
        return 3;
    }
    out[0] = (unsigned char)(0xF0 | (uc >> 18));
    out[1] = (unsigned char)(0x80 | ((uc >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((uc >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (uc & 0x3F));
    return 4;
}
int wctomb(char *s, wchar_t wc) {
    return (int)wcrtomb(s, wc, 0);
}
int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    return (int)mbrtowc(pwc, s, n, 0);
}
size_t mbstowcs(wchar_t *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n) {
        wchar_t wc;
        int len = mbtowc(&wc, src, 4);
        if (len <= 0) {
            break;
        }
        if (dest) {
            dest[i] = wc;
        }
        if (wc == 0) {
            break;
        }
        src += len;
        i++;
    }
    return i;
}
size_t wcstombs(char *dest, const wchar_t *src, size_t n) {
    size_t total = 0;
    while (*src) {
        char tmp[4];
        int len = (int)wcrtomb(tmp, *src, 0);
        if (len <= 0 || total + (size_t)len > n) {
            break;
        }
        if (dest) {
            memcpy(dest + total, tmp, (size_t)len);
        }
        total += (size_t)len;
        src++;
    }
    return total;
}
int wcwidth(wchar_t c) {
    unsigned int uc = (unsigned int)c;
    if (uc == 0) {
        return 0;
    }
    if (uc < 0x20 || (uc >= 0x7f && uc < 0xa0)) {
        return -1;
    }
    if ((uc >= 0x1100 && uc <= 0x115F) || (uc >= 0x2E80 && uc <= 0xA4CF)
        || (uc >= 0xAC00 && uc <= 0xD7A3) || (uc >= 0xF900 && uc <= 0xFAFF)
        || (uc >= 0xFF00 && uc <= 0xFF60) || (uc >= 0x20000 && uc <= 0x3FFFD)) {
        return 2;
    }
    return 1;
}
