#ifndef ROBU_LIBC_NET_IF_H
#define ROBU_LIBC_NET_IF_H
#include <sys/socket.h>
#define IFNAMSIZ 16
struct ifreq {
    char ifr_name[IFNAMSIZ];
    union {
        struct sockaddr ifr_addr;
        int ifr_flags;
    };
};
#endif
