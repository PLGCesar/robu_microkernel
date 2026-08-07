#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
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
    char buf[32];
    check(getcwd(buf, sizeof(buf)) != 0 && strcmp(buf, "/") == 0);
    check(chdir("/dev") == 0);
    check(getcwd(buf, sizeof(buf)) != 0 && strcmp(buf, "/dev") == 0);
    errno = 0;
    check(chdir("/nonexistent") == -1 && errno == ENOTDIR);
    check(getcwd(buf, sizeof(buf)) != 0 && strcmp(buf, "/dev") == 0);
    check(chdir("/") == 0);
    check(getcwd(buf, sizeof(buf)) != 0 && strcmp(buf, "/") == 0);
    int fd = open("/dev/null", O_WRONLY);
    check(fd >= 3);
    check(write(fd, "hi", 2) == 2);
    int fd2 = dup(fd);
    check(fd2 >= 0 && fd2 != fd);
    check(write(fd2, "yo", 2) == 2);
    int target = 20;
    check(dup2(fd, target) == target);
    check(close(fd) == 0);
    check(close(fd2) == 0);
    check(close(target) == 0);
    int fdz = open("/dev/zero", O_RDONLY);
    check(fdz >= 0);
    uint8_t zbuf[16];
    memset(zbuf, 0xff, sizeof(zbuf));
    check(read(fdz, zbuf, sizeof(zbuf)) == 16);
    int all_zero = 1;
    for (int i = 0; i < 16; i++) {
        if (zbuf[i] != 0) {
            all_zero = 0;
        }
    }
    check(all_zero);
    check(close(fdz) == 0);
    errno = 0;
    int badfd = open("/nonexistent", O_RDONLY);
    check(badfd == -1 && errno == ENOENT);
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = TEST_REPORT_KIND_LIBC_FDTEST;
    m.word[1] = g_checks;
    m.word[2] = g_passed;
    m.word[3] = g_failbits;
    ipc_send((tid_t)kinfo_user()->test_report_tid, &m);
    return 0;
}
