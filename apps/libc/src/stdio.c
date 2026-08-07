#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include "libc_internal.h"
struct FILE {
    int fd;
    uint8_t *wbuf;
    size_t wbuf_cap;
    size_t wbuf_len;
    int buf_mode;
    int owns_buf;
    int eof;
    int error;
    const uint8_t *mbuf;
    size_t mlen;
    size_t mpos;
};
static struct FILE stdin_file = { .fd = 0, .buf_mode = _IONBF };
static struct FILE stdout_file = { .fd = 1, .buf_mode = _IOLBF };
static struct FILE stderr_file = { .fd = 2, .buf_mode = _IONBF };
FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;
void __libc_stdio_init(void) {
}
static int raw_flush(FILE *f) {
    if (f->wbuf_len == 0) {
        return 0;
    }
    size_t off = 0;
    while (off < f->wbuf_len) {
        ssize_t n = write(f->fd, f->wbuf + off, f->wbuf_len - off);
        if (n <= 0) {
            f->error = 1;
            f->wbuf_len = 0;
            return EOF;
        }
        off += (size_t)n;
    }
    f->wbuf_len = 0;
    return 0;
}
static int raw_putc(FILE *f, int c) {
    if (f->buf_mode == _IONBF || !f->wbuf) {
        unsigned char ch = (unsigned char)c;
        if (write(f->fd, &ch, 1) != 1) {
            f->error = 1;
            return EOF;
        }
        return (unsigned char)c;
    }
    f->wbuf[f->wbuf_len++] = (unsigned char)c;
    if (f->wbuf_len == f->wbuf_cap || (f->buf_mode == _IOLBF && c == '\n')) {
        if (raw_flush(f) == EOF) {
            return EOF;
        }
    }
    return (unsigned char)c;
}
static size_t raw_write_buf(FILE *f, const void *buf, size_t n) {
    const unsigned char *p = buf;
    for (size_t i = 0; i < n; i++) {
        if (raw_putc(f, p[i]) == EOF) {
            return i;
        }
    }
    return n;
}
void setvbuf(FILE *f, char *buf, int mode, size_t size) {
    raw_flush(f);
    if (f->owns_buf) {
        free(f->wbuf);
    }
    f->buf_mode = mode;
    if (mode == _IONBF || size == 0) {
        f->wbuf = 0;
        f->wbuf_cap = 0;
        f->owns_buf = 0;
    } else {
        f->wbuf = (uint8_t *)buf;
        f->wbuf_cap = size;
        f->owns_buf = 0;
    }
    f->wbuf_len = 0;
}
void setbuf(FILE *f, char *buf) {
    setvbuf(f, buf, buf ? _IOFBF : _IONBF, buf ? BUFSIZ : 0);
}
FILE *fopen(const char *path, const char *mode) {
    (void)path;
    (void)mode;
    errno = ENOSYS;
    return 0;
}
FILE *fdopen(int fd, const char *mode) {
    (void)mode;
    if (fd == 0) {
        return stdin;
    }
    if (fd == 1) {
        return stdout;
    }
    if (fd == 2) {
        return stderr;
    }
    if (!__libc_fd_is_open(fd)) {
        errno = EBADF;
        return 0;
    }
    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        return 0;
    }
    *f = (FILE){ .fd = fd, .buf_mode = _IONBF };
    return f;
}
FILE *fmemopen(void *buf, size_t size, const char *mode) {
    if (!buf || !mode || (mode[0] != 'r')) {
        errno = mode && mode[0] != 'r' ? ENOSYS : EINVAL;
        return 0;
    }
    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        errno = ENOMEM;
        return 0;
    }
    *f = (FILE){ .fd = -1, .buf_mode = _IONBF, .mbuf = buf, .mlen = size };
    return f;
}
int fclose(FILE *f) {
    int rc = raw_flush(f);
    if (f->owns_buf) {
        free(f->wbuf);
    }
    if (f != stdin && f != stdout && f != stderr) {
        free(f);
    }
    return rc;
}
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f) {
    size_t total = size * nmemb;
    size_t n = raw_write_buf(f, ptr, total);
    return size ? n / size : 0;
}
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f) {
    size_t total = size * nmemb;
    if (f->mbuf) {
        size_t avail = f->mlen - f->mpos;
        size_t n = total < avail ? total : avail;
        memcpy(ptr, f->mbuf + f->mpos, n);
        f->mpos += n;
        if (n < total) {
            f->eof = 1;
        }
        return size ? n / size : 0;
    }
    ssize_t n = read(f->fd, ptr, total);
    if (n <= 0) {
        if (n == 0) {
            f->eof = 1;
        } else {
            f->error = 1;
        }
        return 0;
    }
    return size ? (size_t)n / size : 0;
}
int fflush(FILE *f) {
    if (!f) {
        int rc = 0;
        if (raw_flush(stdout) == EOF) rc = EOF;
        if (raw_flush(stderr) == EOF) rc = EOF;
        return rc;
    }
    return raw_flush(f);
}
int fseek(FILE *f, long offset, int whence) {
    (void)f;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}
long ftell(FILE *f) {
    (void)f;
    errno = ESPIPE;
    return -1;
}
void rewind(FILE *f) {
    (void)f;
}
int fileno(FILE *f) {
    return f->fd;
}
int feof(FILE *f) {
    return f->eof;
}
int ferror(FILE *f) {
    return f->error;
}
void clearerr(FILE *f) {
    f->eof = 0;
    f->error = 0;
}
int fputc(int c, FILE *f) {
    return raw_putc(f, c);
}
int putc(int c, FILE *f) {
    return raw_putc(f, c);
}
int putchar(int c) {
    return raw_putc(stdout, c);
}
int fputs(const char *s, FILE *f) {
    size_t len = strlen(s);
    return raw_write_buf(f, s, len) == len ? 0 : EOF;
}
int puts(const char *s) {
    if (fputs(s, stdout) == EOF) {
        return EOF;
    }
    return raw_putc(stdout, '\n') == EOF ? EOF : 0;
}
int fgetc(FILE *f) {
    if (f->mbuf) {
        if (f->mpos >= f->mlen) {
            f->eof = 1;
            return EOF;
        }
        return f->mbuf[f->mpos++];
    }
    unsigned char c;
    ssize_t n = read(f->fd, &c, 1);
    if (n <= 0) {
        if (n == 0) f->eof = 1; else f->error = 1;
        return EOF;
    }
    return c;
}
int getc(FILE *f) {
    return fgetc(f);
}
int getchar(void) {
    return fgetc(stdin);
}
int ungetc(int c, FILE *f) {
    (void)f;
    (void)c;
    return EOF;
}
char *fgets(char *s, int size, FILE *f) {
    if (size <= 0) {
        return 0;
    }
    int i = 0;
    for (; i < size - 1; i++) {
        int c = fgetc(f);
        if (c == EOF) {
            if (i == 0) {
                return 0;
            }
            break;
        }
        s[i] = (char)c;
        if (c == '\n') {
            i++;
            break;
        }
    }
    s[i] = '\0';
    return s;
}
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *f) {
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
    }
    size_t len = 0;
    for (;;) {
        int c = fgetc(f);
        if (c == EOF) {
            if (len == 0) {
                return -1;
            }
            break;
        }
        if (len + 1 >= *n) {
            *n *= 2;
            *lineptr = realloc(*lineptr, *n);
        }
        (*lineptr)[len++] = (char)c;
        if (c == delim) {
            break;
        }
    }
    (*lineptr)[len] = '\0';
    return (ssize_t)len;
}
ssize_t getline(char **lineptr, size_t *n, FILE *f) {
    return getdelim(lineptr, n, '\n', f);
}
int sscanf(const char *str, const char *fmt, ...) {
    (void)str;
    (void)fmt;
    return -1;
}
int vsscanf(const char *str, const char *fmt, va_list ap) {
    (void)str;
    (void)fmt;
    (void)ap;
    return -1;
}
int scanf(const char *fmt, ...) {
    (void)fmt;
    return -1;
}
struct sink {
    char *buf;
    size_t cap;
    size_t pos;
    FILE *f;
};
static void sink_putc(struct sink *sk, char c) {
    if (sk->f) {
        raw_putc(sk->f, (unsigned char)c);
    } else if (sk->buf) {
        if (sk->pos < sk->cap) {
            sk->buf[sk->pos] = c;
        }
    }
    sk->pos++;
}
static void sink_puts(struct sink *sk, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        sink_putc(sk, s[i]);
    }
}
static void out_padded(struct sink *sk, const char *s, size_t len, int width,
                       int left, char pad) {
    int padlen = width > (int)len ? width - (int)len : 0;
    if (!left) {
        for (int i = 0; i < padlen; i++) sink_putc(sk, pad);
    }
    sink_puts(sk, s, len);
    if (left) {
        for (int i = 0; i < padlen; i++) sink_putc(sk, ' ');
    }
}
static int format_uint(unsigned long long v, int base, int upper, char *out) {
    static const char *digits_lo = "0123456789abcdef";
    static const char *digits_hi = "0123456789ABCDEF";
    const char *digits = upper ? digits_hi : digits_lo;
    char tmp[32];
    int n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    }
    while (v) {
        tmp[n++] = digits[v % (unsigned)base];
        v /= (unsigned)base;
    }
    for (int i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}
static int vsnprintf_core(struct sink *sk, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            sink_putc(sk, *fmt);
            continue;
        }
        fmt++;
        int left = 0, zero = 0, plus = 0, space = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '#') {  }
            else break;
        }
        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }
        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    prec = prec * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }
        int lmod = 0;
        for (;;) {
            if (*fmt == 'l') { lmod++; fmt++; }
            else if (*fmt == 'h') { lmod--; fmt++; }
            else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') { lmod = 1; fmt++; }
            else break;
        }
        char c = *fmt;
        char numbuf[32];
        char pad = zero && !left ? '0' : ' ';
        switch (c) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t len = strlen(s);
            if (prec >= 0 && (size_t)prec < len) len = (size_t)prec;
            out_padded(sk, s, len, width, left, ' ');
            break;
        }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            out_padded(sk, &ch, 1, width, left, ' ');
            break;
        }
        case 'd':
        case 'i': {
            long long v;
            if (lmod >= 2) v = va_arg(ap, long long);
            else if (lmod == 1) v = va_arg(ap, long);
            else v = va_arg(ap, int);
            int neg = v < 0;
            unsigned long long uv = neg ? (unsigned long long)(-v) : (unsigned long long)v;
            int n = format_uint(uv, 10, 0, numbuf + 1);
            char sign = neg ? '-' : (plus ? '+' : (space ? ' ' : 0));
            char full[34];
            int flen = 0;
            if (sign) full[flen++] = sign;
            memcpy(full + flen, numbuf + 1, n);
            flen += n;
            out_padded(sk, full, flen, width, left, pad);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o': {
            unsigned long long v;
            if (lmod >= 2) v = va_arg(ap, unsigned long long);
            else if (lmod == 1) v = va_arg(ap, unsigned long);
            else v = va_arg(ap, unsigned int);
            int base = c == 'o' ? 8 : (c == 'u' ? 10 : 16);
            int n = format_uint(v, base, c == 'X', numbuf);
            out_padded(sk, numbuf, n, width, left, pad);
            break;
        }
        case 'p': {
            unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            char full[20];
            full[0] = '0';
            full[1] = 'x';
            int n = format_uint(v, 16, 0, full + 2);
            out_padded(sk, full, n + 2, width, left, ' ');
            break;
        }
        case '%':
            sink_putc(sk, '%');
            break;
        case 0:
            fmt--;
            break;
        default:
            sink_putc(sk, '%');
            sink_putc(sk, c);
            break;
        }
    }
    return (int)sk->pos;
}
int vfprintf(FILE *f, const char *fmt, va_list ap) {
    struct sink sk = { .f = f };
    return vsnprintf_core(&sk, fmt, ap);
}
int vprintf(const char *fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    struct sink sk = { .buf = buf, .cap = size ? size - 1 : 0 };
    int n = vsnprintf_core(&sk, fmt, ap);
    if (size) {
        buf[sk.pos < sk.cap ? sk.pos : sk.cap] = '\0';
    }
    return n;
}
int vsprintf(char *buf, const char *fmt, va_list ap) {
    return vsnprintf(buf, (size_t)-1, fmt, ap);
}
int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}
int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}
int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
int vdprintf(int fd, const char *fmt, va_list ap) {
    char tmp[256];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    write(fd, tmp, (size_t)(n < (int)sizeof(tmp) ? n : (int)sizeof(tmp) - 1));
    return n;
}
int dprintf(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vdprintf(fd, fmt, ap);
    va_end(ap);
    return n;
}
void perror(const char *s) {
    if (s && *s) {
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    } else {
        fprintf(stderr, "%s\n", strerror(errno));
    }
}
int remove(const char *path) {
    return unlink(path);
}
