#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

extern int __libc_spawn(const char *name, char *const argv[], char *const envp[]);

int main(int argc, char **argv) {
    printf("[hello_initsys] init starting\n");

    int have_child = 0;
    if (argc > 1) {
        int rc = __libc_spawn(argv[1], argv + 1, environ);
        if (rc < 0) {
            printf("[hello_initsys] failed to start '%s'\n", argv[1]);
        } else {
            printf("[hello_initsys] started '%s' as tid=%d\n", argv[1], rc);
            have_child = 1;
        }
    } else {
        printf("[hello_initsys] no command given, idling\n");
    }

    for (;;) {
        if (!have_child) {
            sleep(1);
            continue;
        }
        int status;
        pid_t child = wait(&status);
        if (child > 0) {
            printf("[hello_initsys] tid=%d exited status=%d\n", child, WEXITSTATUS(status));
        } else {
            have_child = 0;
        }
    }
}
