#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
char *getenv(const char *name) {
    if (!environ) {
        return 0;
    }
    size_t len = strlen(name);
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, len) == 0 && (*e)[len] == '=') {
            return *e + len + 1;
        }
    }
    return 0;
}
int setenv(const char *name, const char *value, int overwrite) {
    (void)name;
    (void)value;
    (void)overwrite;
    errno = ENOSYS;
    return -1;
}
int unsetenv(const char *name) {
    (void)name;
    errno = ENOSYS;
    return -1;
}
