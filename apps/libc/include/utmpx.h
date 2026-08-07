#ifndef ROBU_LIBC_UTMPX_H
#define ROBU_LIBC_UTMPX_H

#include <sys/types.h>
#include <sys/time.h>

#define EMPTY 0
#define RUN_LVL 1
#define BOOT_TIME 2
#define NEW_TIME 3
#define OLD_TIME 4
#define INIT_PROCESS 5
#define LOGIN_PROCESS 6
#define USER_PROCESS 7
#define DEAD_PROCESS 8
#define ACCOUNTING 9

struct utmpx {
    short ut_type;
    pid_t ut_pid;
    char ut_line[16];
    char ut_id[8];
    char ut_user[32];
    char ut_host[128];
    struct timeval ut_tv;
};

static inline void setutxent(void) {}
static inline void endutxent(void) {}
static inline struct utmpx *getutxent(void) { return 0; }

#endif
