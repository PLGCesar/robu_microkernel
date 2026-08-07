#ifndef ROBU_LIBC_ARPA_INET_H
#define ROBU_LIBC_ARPA_INET_H
#include <sys/types.h>
unsigned short htons(unsigned short x);
unsigned short ntohs(unsigned short x);
unsigned int htonl(unsigned int x);
unsigned int ntohl(unsigned int x);
char *inet_ntoa(unsigned int in);
int inet_aton(const char *cp, void *addr);
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, unsigned int size);
#endif
