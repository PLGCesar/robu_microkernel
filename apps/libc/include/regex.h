#ifndef ROBU_LIBC_REGEX_H
#define ROBU_LIBC_REGEX_H
typedef struct {
    int re_nsub;
    void *internal;
} regex_t;
typedef struct {
    long rm_so;
    long rm_eo;
} regmatch_t;
#define REG_EXTENDED (1 << 0)
#define REG_STARTEND (1 << 1)
#define REG_NOSUB    (1 << 2)
#define REG_NOMATCH  1
int regcomp(regex_t *preg, const char *pattern, int cflags);
int regexec(const regex_t *preg, const char *string, unsigned long nmatch,
           regmatch_t pmatch[], int eflags);
unsigned long regerror(int errcode, const regex_t *preg, char *errbuf, unsigned long errbuf_size);
void regfree(regex_t *preg);
#endif
