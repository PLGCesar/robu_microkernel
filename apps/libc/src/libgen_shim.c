#include <libgen.h>
#include <string.h>
char *basename(char *path) {
    if (!path || !*path) {
        return (char *)".";
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
    if (len == 1 && path[0] == '/') {
        return path;
    }
    char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
char *dirname(char *path) {
    static char dot[] = ".";
    if (!path || !*path) {
        return dot;
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
    char *slash = strrchr(path, '/');
    if (!slash) {
        return dot;
    }
    if (slash == path) {
        path[1] = '\0';
        return path;
    }
    *slash = '\0';
    return path;
}
