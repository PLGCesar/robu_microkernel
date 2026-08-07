#ifndef ROBU_LIBC_GRP_H
#define ROBU_LIBC_GRP_H
#include <sys/types.h>
struct group {
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};
struct group *getgrgid(gid_t gid);
struct group *getgrnam(const char *name);
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result);
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen, struct group **result);
#endif
