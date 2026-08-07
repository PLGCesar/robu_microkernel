#ifndef ROBU_LIBC_STDIO_H
#define ROBU_LIBC_STDIO_H
#include <sys/types.h>
#include <stdarg.h>
#ifndef NULL
#define NULL ((void *)0)
#endif
#define EOF (-1)
#define BUFSIZ 1024
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
typedef struct FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *fmemopen(void *buf, size_t size, const char *mode);
int fscanf(FILE *f, const char *fmt, ...);
int fclose(FILE *f);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
int fflush(FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
void rewind(FILE *f);
int fileno(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int puts(const char *s);
int putchar(int c);
int fgetc(FILE *f);
char *fgets(char *s, int size, FILE *f);
int getchar(void);
int ungetc(int c, FILE *f);
int putc(int c, FILE *f);
int getc(FILE *f);
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *f);
ssize_t getline(char **lineptr, size_t *n, FILE *f);
int sscanf(const char *str, const char *fmt, ...);
int vsscanf(const char *str, const char *fmt, va_list ap);
int scanf(const char *fmt, ...);
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);
int vdprintf(int fd, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
void perror(const char *s);
void setvbuf(FILE *f, char *buf, int mode, size_t size);
void setbuf(FILE *f, char *buf);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
#endif
