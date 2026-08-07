#ifndef ROBU_LIBC_NETDB_H
#define ROBU_LIBC_NETDB_H
#include <netinet/in.h>
struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};
#define AI_PASSIVE 0x0001
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
struct hostent *gethostbyname(const char *name);
const char *gai_strerror(int errcode);
#endif
