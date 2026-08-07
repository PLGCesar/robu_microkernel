#include <wctype.h>
int iswspace(wint_t wc) { return wc == ' ' || wc == '\t' || wc == '\n' || wc == '\v' || wc == '\f' || wc == '\r'; }
int iswalpha(wint_t wc) { return (wc >= 'A' && wc <= 'Z') || (wc >= 'a' && wc <= 'z'); }
int iswdigit(wint_t wc) { return wc >= '0' && wc <= '9'; }
int iswprint(wint_t wc) { return wc >= 0x20 && wc < 0x7f; }
int iswpunct(wint_t wc) { return wc > 0x20 && wc < 0x7f && !iswalpha(wc) && !iswdigit(wc); }
int iswcntrl(wint_t wc) { return (wc >= 0 && wc < 0x20) || wc == 0x7f; }
int iswupper(wint_t wc) { return wc >= 'A' && wc <= 'Z'; }
int iswlower(wint_t wc) { return wc >= 'a' && wc <= 'z'; }
wint_t towupper(wint_t wc) { return iswlower(wc) ? wc - 'a' + 'A' : wc; }
wint_t towlower(wint_t wc) { return iswupper(wc) ? wc - 'A' + 'a' : wc; }
