#include <string.h>
#include <errno.h>
#include <stdlib.h>
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = a, *pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            return (int)pa[i] - (int)pb[i];
        }
    }
    return 0;
}
void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (unsigned char)c) {
            return (void *)(p + i);
        }
    }
    return 0;
}
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) { }
    return dest;
}
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}
char *strcat(char *dest, const char *src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}
char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest + strlen(dest);
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        d[i] = src[i];
    }
    d[i] = '\0';
    return dest;
}
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) {
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        }
    }
    return 0;
}
static int lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}
int strcasecmp(const char *a, const char *b) {
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) {
        a++;
        b++;
    }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}
int strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = lower((unsigned char)a[i]), cb = lower((unsigned char)b[i]);
        if (ca != cb || !a[i]) {
            return ca - cb;
        }
    }
    return 0;
}
char *strchr(const char *s, int c) {
    for (; *s; s++) {
        if (*s == (char)c) {
            return (char *)s;
        }
    }
    return c == 0 ? (char *)s : 0;
}
char *strrchr(const char *s, int c) {
    const char *last = c == 0 ? s + strlen(s) : 0;
    for (; *s; s++) {
        if (*s == (char)c) {
            last = s;
        }
    }
    return (char *)last;
}
char *strstr(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, nlen) == 0) {
            return (char *)haystack;
        }
    }
    return 0;
}
char *strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) {
        memcpy(d, s, n);
    }
    return d;
}
char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) {
        len++;
    }
    char *d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}
char *stpcpy(char *dest, const char *src) {
    while ((*dest = *src)) {
        dest++;
        src++;
    }
    return dest;
}
char *stpncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    char *end = dest + i;
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return end;
}
char *strtok_r(char *s, const char *delim, char **saveptr) {
    if (!s) {
        s = *saveptr;
    }
    s += strspn(s, delim);
    if (!*s) {
        *saveptr = s;
        return 0;
    }
    char *tok = s;
    s += strcspn(s, delim);
    if (*s) {
        *s = '\0';
        s++;
    }
    *saveptr = s;
    return tok;
}
char *strtok(char *s, const char *delim) {
    static char *saveptr;
    return strtok_r(s, delim, &saveptr);
}
size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) {
        n++;
    }
    return n;
}
size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) {
        n++;
    }
    return n;
}
char *strpbrk(const char *s, const char *accept) {
    for (; *s; s++) {
        if (strchr(accept, *s)) {
            return (char *)s;
        }
    }
    return 0;
}
size_t strlcpy(char *dest, const char *src, size_t size) {
    size_t srclen = strlen(src);
    if (size) {
        size_t n = srclen < size - 1 ? srclen : size - 1;
        memcpy(dest, src, n);
        dest[n] = '\0';
    }
    return srclen;
}
size_t strlcat(char *dest, const char *src, size_t size) {
    size_t dlen = strlen(dest);
    if (dlen >= size) {
        return dlen + strlen(src);
    }
    return dlen + strlcpy(dest + dlen, src, size - dlen);
}
static const char *const errno_msgs[] = {
    [0] = "Success",
    [EPERM] = "Operation not permitted",
    [ENOENT] = "No such file or directory",
    [ESRCH] = "No such process",
    [ECHILD] = "No child processes",
    [EINTR] = "Interrupted system call",
    [EIO] = "Input/output error",
    [ENXIO] = "No such device or address",
    [E2BIG] = "Argument list too long",
    [ENOEXEC] = "Exec format error",
    [EBADF] = "Bad file descriptor",
    [EAGAIN] = "Resource temporarily unavailable",
    [ENOMEM] = "Cannot allocate memory",
    [EACCES] = "Permission denied",
    [EFAULT] = "Bad address",
    [EBUSY] = "Device or resource busy",
    [EEXIST] = "File exists",
    [EXDEV] = "Invalid cross-device link",
    [ENODEV] = "No such device",
    [ENOTDIR] = "Not a directory",
    [EISDIR] = "Is a directory",
    [EINVAL] = "Invalid argument",
    [ENFILE] = "Too many open files in system",
    [EMFILE] = "Too many open files",
    [ENOTTY] = "Inappropriate ioctl for device",
    [EFBIG] = "File too large",
    [ENOSPC] = "No space left on device",
    [ESPIPE] = "Illegal seek",
    [EROFS] = "Read-only file system",
    [EMLINK] = "Too many links",
    [EPIPE] = "Broken pipe",
    [ERANGE] = "Result too large",
    [ENAMETOOLONG] = "File name too long",
    [ENOSYS] = "Function not implemented",
    [ENOTEMPTY] = "Directory not empty",
    [ELOOP] = "Too many levels of symbolic links",
    [EILSEQ] = "Invalid or incomplete multibyte or wide character",
    [ENOTSUP] = "Operation not supported",
};
char *strerror(int errnum) {
    if (errnum >= 0 && (size_t)errnum < sizeof(errno_msgs) / sizeof(errno_msgs[0])
        && errno_msgs[errnum]) {
        return (char *)errno_msgs[errnum];
    }
    return (char *)"Unknown error";
}
