#ifndef ROBU_LIBC_STDLIB_H
#define ROBU_LIBC_STDLIB_H
#include <sys/types.h>
#ifndef NULL
#define NULL ((void *)0)
#endif
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MB_CUR_MAX 4
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atoi(const char *s);
long atol(const char *s);
long long atoll(const char *s);
long strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);
long long strtoll(const char *s, char **endptr, int base);
unsigned long long strtoull(const char *s, char **endptr, int base);
double strtod(const char *s, char **endptr);
long double strtold(const char *s, char **endptr);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *));
long random(void);
void srandom(unsigned int seed);
int abs(int j);
long labs(long j);
int system(const char *command);
char *realpath(const char *path, char *resolved_path);
char *mkdtemp(char *template_str);
int mkstemp(char *template_str);

extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char *const argv[], const char *optstring);

#endif
