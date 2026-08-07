#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
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
    int fd = open("/testfile.txt", O_WRONLY | O_CREAT | O_TRUNC);
    check(fd >= 0);
    check(write(fd, "hello ramfs", 11) == 11);
    check(close(fd) == 0);
    int fd2 = open("/testfile.txt", O_RDONLY);
    check(fd2 >= 0);
    char buf[32] = {0};
    check(read(fd2, buf, sizeof(buf)) == 11);
    check(strcmp(buf, "hello ramfs") == 0);
    check(close(fd2) == 0);
    struct stat st;
    check(stat("/testfile.txt", &st) == 0);
    check(st.st_size == 11);
    check(S_ISREG(st.st_mode));
    DIR *d = opendir("/");
    check(d != 0);
    int found = 0;
    struct dirent *ent;
    while (d && (ent = readdir(d)) != 0) {
        if (strcmp(ent->d_name, "testfile.txt") == 0) {
            found = 1;
        }
    }
    check(found);
    if (d) {
        closedir(d);
    }
    check(rename("/testfile.txt", "/renamed.txt") == 0);
    check(stat("/renamed.txt", &st) == 0 && st.st_size == 11);
    errno = 0;
    check(stat("/testfile.txt", &st) == -1 && errno == ENOENT);
    check(unlink("/renamed.txt") == 0);
    errno = 0;
    check(stat("/renamed.txt", &st) == -1 && errno == ENOENT);
    errno = 0;
    check(open("/nonexistent.txt", O_RDONLY) == -1 && errno == ENOENT);
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = TEST_REPORT_KIND_RAMFS_TEST;
    m.word[1] = g_checks;
    m.word[2] = g_passed;
    m.word[3] = g_failbits;
    ipc_send((tid_t)kinfo_user()->test_report_tid, &m);
    return 0;
}
