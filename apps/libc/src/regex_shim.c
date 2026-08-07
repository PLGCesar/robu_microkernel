#include <regex.h>
int regcomp(regex_t *preg, const char *pattern, int cflags) {
    (void)pattern;
    (void)cflags;
    if (preg) {
        preg->re_nsub = 0;
        preg->internal = 0;
    }
    return 0;
}
int regexec(const regex_t *preg, const char *string, unsigned long nmatch,
           regmatch_t pmatch[], int eflags) {
    (void)preg;
    (void)string;
    (void)nmatch;
    (void)pmatch;
    (void)eflags;
    return REG_NOMATCH;
}
unsigned long regerror(int errcode, const regex_t *preg, char *errbuf, unsigned long errbuf_size) {
    (void)errcode;
    (void)preg;
    static const char msg[] = "regex not supported on this kernel";
    unsigned long len = sizeof(msg);
    if (errbuf && errbuf_size) {
        unsigned long n = len < errbuf_size ? len : errbuf_size;
        for (unsigned long i = 0; i + 1 < n; i++) {
            errbuf[i] = msg[i];
        }
        errbuf[n - 1] = '\0';
    }
    return len;
}
void regfree(regex_t *preg) {
    (void)preg;
}
