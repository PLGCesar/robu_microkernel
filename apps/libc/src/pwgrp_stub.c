#include <pwd.h>
#include <grp.h>
#include <errno.h>
struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return 0;
}
struct passwd *getpwnam(const char *name) {
    (void)name;
    return 0;
}
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    (void)uid;
    (void)pwd;
    (void)buf;
    (void)buflen;
    *result = 0;
    return 0;
}
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    (void)name;
    (void)pwd;
    (void)buf;
    (void)buflen;
    *result = 0;
    return 0;
}
struct group *getgrgid(gid_t gid) {
    (void)gid;
    return 0;
}
struct group *getgrnam(const char *name) {
    (void)name;
    return 0;
}
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result) {
    (void)gid;
    (void)grp;
    (void)buf;
    (void)buflen;
    *result = 0;
    return 0;
}
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen, struct group **result) {
    (void)name;
    (void)grp;
    (void)buf;
    (void)buflen;
    *result = 0;
    return 0;
}
