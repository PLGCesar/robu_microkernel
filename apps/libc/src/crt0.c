#include <stdlib.h>
#include <unistd.h>
#include "libc_internal.h"
char **environ;
const char *__libc_progname;
static char *empty_environ[1];
extern int main(int argc, char **argv);
void _start(uint64_t argc, uint64_t argv, uint64_t envp, uint64_t heap_base,
           uint64_t spawn_info) {
    char **argv_arr = (char **)argv;
    __libc_heap_init(heap_base);
    environ = envp ? (char **)envp : empty_environ;
    __libc_progname = (argc > 0 && argv_arr) ? argv_arr[0] : 0;
    for (char **e = environ; *e; e++) {
        if ((*e)[0] == 'P' && (*e)[1] == 'W' && (*e)[2] == 'D' && (*e)[3] == '=') {
            chdir(*e + 4);
            break;
        }
    }
    __libc_fd_inherit(spawn_info);
    __libc_stdio_init();
    int rc = main((int)argc, argv_arr);
    exit(rc);
}
