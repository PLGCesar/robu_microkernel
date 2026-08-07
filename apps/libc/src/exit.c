#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
#include "libc_internal.h"
void _exit(int status) {
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = TEST_REPORT_KIND_EXIT;
    m.word[1] = (uint64_t)(int64_t)status;
    const char *name = __libc_progname;
    uint64_t w2 = 0, w3 = 0;
    if (name) {
        int i = 0;
        for (; i < 8 && name[i]; i++) {
            w2 |= ((uint64_t)(uint8_t)name[i]) << (8 * i);
        }
        if (i == 8) {
            for (int j = 0; j < 8 && name[8 + j]; j++) {
                w3 |= ((uint64_t)(uint8_t)name[8 + j]) << (8 * j);
            }
        }
    }
    m.word[2] = w2;
    m.word[3] = w3;
    tid_t report_tid = (tid_t)kinfo_user()->test_report_tid;
    if (report_tid) {
        ipc_send(report_tid, &m);
    }
    ipc_exit(status);
}
void exit(int status) {
    fflush(NULL);
    _exit(status);
}
void abort(void) {
    fflush(NULL);
    _exit(134);
}
