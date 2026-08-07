#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
int __libc_spawn(const char *name, char *const argv[], char *const envp[]);
static uint64_t g_checks = 0, g_passed = 0, g_failbits = 0;
static void check(int ok) {
    if (ok) {
        g_passed++;
    } else {
        g_failbits |= (1ULL << g_checks);
    }
    g_checks++;
}
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char *const true_argv[] = { "true", 0 };
    int tid = __libc_spawn("true", true_argv, 0);
    check(tid > 0);
    waitpid(tid, 0, 0);
    char *const bogus_argv[] = { "this-command-does-not-exist", 0 };
    errno = 0;
    int rc_missing = __libc_spawn("this-command-does-not-exist", bogus_argv, 0);
    check(rc_missing == -1 && errno == ENOENT);
    char *big_argv[70];
    for (int i = 0; i < 69; i++) {
        big_argv[i] = "x";
    }
    big_argv[69] = 0;
    errno = 0;
    int rc_big = __libc_spawn("true", big_argv, 0);
    check(rc_big == -1 && errno == E2BIG);
    char *const at_argv[] = { "argvtest", "hello-from-spawntest", 0 };
    char *const at_envp[] = { "FOO=bar", 0 };
    int tid2 = __libc_spawn("argvtest", at_argv, at_envp);
    check(tid2 > 0);
    int true_tid = __libc_spawn("true", true_argv, 0);
    int wstatus = -1;
    pid_t reaped = waitpid(true_tid, &wstatus, 0);
    check(reaped == true_tid && WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
    char *const false_argv[] = { "false", 0 };
    int false_tid = __libc_spawn("false", false_argv, 0);
    wstatus = -1;
    reaped = waitpid(false_tid, &wstatus, 0);
    check(reaped == false_tid && WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 1);
    int three[3];
    for (int i = 0; i < 3; i++) {
        three[i] = __libc_spawn("true", true_argv, 0);
    }
    int reaped_count = 0;
    for (int i = 0; i < 3; i++) {
        pid_t r = waitpid(-1, 0, 0);
        for (int j = 0; j < 3; j++) {
            if (three[j] == r) {
                reaped_count++;
                break;
            }
        }
    }
    check(reaped_count == 3);
    errno = 0;
    pid_t rc_none = waitpid(99999, 0, 0);
    check(rc_none == -1 && errno == ECHILD);
    int wnohang_tid = __libc_spawn("true", true_argv, 0);
    pid_t rc_wnohang = waitpid(wnohang_tid, 0, WNOHANG);
    check(rc_wnohang == 0);
    pid_t rc_block = waitpid(wnohang_tid, 0, 0);
    check(rc_block == wnohang_tid);
    int spawn_wait_loop_ok = 1;
    for (int i = 0; i < 40; i++) {
        int t = __libc_spawn("true", true_argv, 0);
        if (t <= 0) {
            spawn_wait_loop_ok = 0;
            break;
        }
        if (waitpid(t, 0, 0) != t) {
            spawn_wait_loop_ok = 0;
            break;
        }
    }
    check(spawn_wait_loop_ok);
    int rfd = open("/spawntest-redirect.txt", O_WRONLY | O_CREAT | O_TRUNC);
    check(rfd >= 0);
    int saved_stdout = dup(STDOUT_FILENO);
    check(saved_stdout >= 0);
    check(dup2(rfd, STDOUT_FILENO) == STDOUT_FILENO);
    close(rfd);
    char *const pwd_argv[] = { "pwd", 0 };
    int pwd_tid = __libc_spawn("pwd", pwd_argv, 0);
    check(pwd_tid > 0);
    waitpid(pwd_tid, 0, 0);
    check(dup2(saved_stdout, STDOUT_FILENO) == STDOUT_FILENO);
    close(saved_stdout);
    int rfd2 = open("/spawntest-redirect.txt", O_RDONLY);
    check(rfd2 >= 0);
    char rbuf[32] = {0};
    ssize_t n = read(rfd2, rbuf, sizeof(rbuf) - 1);
    close(rfd2);
    check(n > 0 && rbuf[0] == '/');
    char *const pwd_argv2[] = { "pwd", 0 };
    int default_tid = __libc_spawn("pwd", pwd_argv2, 0);
    check(default_tid > 0);
    int wstatus2 = -1;
    pid_t default_reaped = waitpid(default_tid, &wstatus2, 0);
    check(default_reaped == default_tid && WIFEXITED(wstatus2) && WEXITSTATUS(wstatus2) == 0);
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = TEST_REPORT_KIND_SPAWN_TEST;
    m.word[1] = g_checks;
    m.word[2] = g_passed;
    m.word[3] = g_failbits;
    ipc_send((tid_t)kinfo_user()->test_report_tid, &m);
    return 0;
}
