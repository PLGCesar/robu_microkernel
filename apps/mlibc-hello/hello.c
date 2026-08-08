#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

static void fork_and_pipe_test(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        printf("pipe() failed: %s\n", strerror(errno));
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("fork() failed: %s\n", strerror(errno));
        return;
    }

    if (pid == 0) {
        close(fds[0]);
        char msg[64];
        int len = snprintf(msg, sizeof(msg), "hello from child pid=%d\n", (int)getpid());
        write(fds[1], msg, (size_t)len);
        close(fds[1]);
        _exit(42);
    }

    close(fds[1]);
    char buf[128];
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    if (n < 0) {
        printf("read from pipe failed: %s\n", strerror(errno));
    } else {
        buf[n] = '\0';
        printf("parent read from child: %s", buf);
    }
    close(fds[0]);

    int status;
    pid_t reaped = waitpid(pid, &status, 0);
    printf("waitpid returned %d, child exit status=%d\n", (int)reaped,
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

extern int __libc_spawn(const char *name, char *const argv[], char *const envp[]);
extern char **environ;

// Classic fork()+exec()+waitpid() pattern: a real child process, whose
// image gets fully replaced by a *different* program (minibox's `touch`,
// aliased in the rootfs -- unlike most other minibox applets, which are
// only reachable via argv[0] dispatch through `minibox`, not as standalone
// rootfs entries). Verified two ways: the child's exit status, and the real
// side effect (the file existing afterward) -- if execve() had silently
// failed and the child just fell through to running more of hello's own
// code, neither would be true.
static void exec_test(void) {
    const char *exec_path = "/var/tmp/exec-test.txt";
    remove(exec_path);

    pid_t pid = fork();
    if (pid < 0) {
        printf("exec test: fork() failed: %s\n", strerror(errno));
        return;
    }

    if (pid == 0) {
        char *const argv[] = { "touch", (char *)exec_path, 0 };
        execve("touch", argv, environ);
        // _exit() (unlike exit()) does not flush stdio -- fflush explicitly
        // so this diagnostic actually reaches the console instead of being
        // silently discarded along with the rest of this process's buffer.
        printf("exec test (child): execve() failed: %s\n", strerror(errno));
        fflush(stdout);
        _exit(123);
    }

    int status;
    pid_t reaped = waitpid(pid, &status, 0);
    printf("exec test: child (tid=%d) reaped=%d exit status=%d (expect 0)\n", (int)pid,
           (int)reaped, WIFEXITED(status) ? WEXITSTATUS(status) : -1);

    struct stat st;
    printf("exec test: %s %s\n", exec_path,
           stat(exec_path, &st) == 0 ? "exists (execve really replaced the image)"
                                      : "MISSING (execve did not actually run touch)");
}

// Simulates the real `producer | cat` shell-pipeline shape: inject data
// directly into a pipe (standing in for a producer process), redirect a
// *spawned* child's stdin onto the read end via dup2 (exactly what a real
// shell does before calling __libc_spawn(), since there's no fork()+exec()
// image-replace here -- see docs/libc-and-abi-reference.md §4.3), and check
// the child (minibox's own `cat`) actually sees the data on its stdin.
static void spawn_pipeline_test(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        printf("pipeline: pipe() failed: %s\n", strerror(errno));
        return;
    }

    const char *msg = "piped through a real pipe\n";
    write(fds[1], msg, strlen(msg));
    close(fds[1]);

    int saved_stdin = dup(0);
    if (dup2(fds[0], 0) != 0) {
        printf("pipeline: dup2 failed: %s\n", strerror(errno));
        close(fds[0]);
        return;
    }
    close(fds[0]);

    char *const cat_argv[] = { "cat", 0 };
    int child = __libc_spawn("cat", cat_argv, environ);

    dup2(saved_stdin, 0);
    close(saved_stdin);

    if (child < 0) {
        printf("pipeline: spawn(cat) failed\n");
        return;
    }

    int status;
    pid_t reaped = waitpid(child, &status, 0);
    printf("pipeline: cat (tid=%d) reaped=%d status=%d\n", child, (int)reaped,
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int main(int argc, char **argv) {
    printf("hello from mlibc on robu\n");
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    const char *path = "/var/tmp/mlibc-hello.txt";
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("fopen(w) failed: %s\n", strerror(errno));
        return 1;
    }
    fputs("hello from mlibc file io\n", f);
    fclose(f);

    struct stat st;
    if (stat(path, &st) != 0) {
        printf("stat failed: %s\n", strerror(errno));
        return 1;
    }
    printf("stat: size=%ld mode=0%o\n", (long)st.st_size, (unsigned)st.st_mode & 0777);

    f = fopen(path, "r");
    if (!f) {
        printf("fopen(r) failed: %s\n", strerror(errno));
        return 1;
    }
    char line[128];
    if (fgets(line, sizeof(line), f)) {
        printf("read back: %s", line);
    }
    fclose(f);

    DIR *d = opendir("/var/tmp");
    if (!d) {
        printf("opendir failed: %s\n", strerror(errno));
        return 1;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        printf("entry: %s\n", de->d_name);
    }
    closedir(d);

    char cwd[64];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("cwd: %s\n", cwd);
    }

    fork_and_pipe_test();
    spawn_pipeline_test();
    exec_test();

    printf("mlibc-hello: all checks done\n");
    return 0;
}
