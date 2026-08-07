#ifndef ROBU_LIBC_NETINET_IN_H
#define ROBU_LIBC_NETINET_IN_H
#include <sys/socket.h>
struct in_addr {
    unsigned int s_addr;
};
struct sockaddr_in {
    sa_family_t sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
struct in6_addr {
    unsigned char s6_addr[16];
};
struct sockaddr_in6 {
    sa_family_t sin6_family;
    unsigned short sin6_port;
    unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int sin6_scope_id;
};
#define INADDR_ANY ((unsigned int)0)
#endif
