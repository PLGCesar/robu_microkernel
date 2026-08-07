#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

extern int __libc_spawn(const char *name, char *const argv[], char *const envp[]);

int main(int argc, char **argv) {
    printf("[hello_initsys] init starting\n");

    char *default_argv[] = { (char *)"sh", 0 };
    const char *name;
    char **cmd_argv;
    if (argc > 1) {
        name = argv[1];
        cmd_argv = argv + 1;
    } else {
        name = "sh";
        cmd_argv = default_argv;
    }

    for (;;) {
        int rc = __libc_spawn(name, cmd_argv, environ);
        if (rc < 0) {
            printf("[hello_initsys] failed to start '%s'\n", name);
            sleep(1);
            continue;
        }
        printf("[hello_initsys] started '%s' as tid=%d\n", name, rc);
        for (;;) {
            int status;
            pid_t child = wait(&status);
            if (child < 0) {
                break;
            }
            printf("[hello_initsys] tid=%d exited status=%d\n", child, WEXITSTATUS(status));
            if (child == rc) {
                break;
            }
        }
    }
}
