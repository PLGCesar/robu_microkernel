#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char *expr, const char *file, int line, const char *func) {
    fprintf(stderr, "assertion failed: %s (%s: %s: line %d)\n", expr, file, func, line);
    abort();
}
