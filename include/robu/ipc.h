#ifndef ROBU_IPC_H
#define ROBU_IPC_H
#include "robu/types.h"
#include "robu/tcb.h"
#define IPC_TID_ANY 0
#define IPC_FLAG_NONE 0
#define IPC_FLAG_MAP  (1 << 0)
#define IPC_FLAG_RECV (1 << 1)
#define IPC_FLAG_XFER (1 << 2)
#define IPC_FLAG_SHARE (1 << 3)
#define IPC_FLAG_NOBLOCK (1 << 4)
#define IPC_FLAG_RETYPE  (1 << 5)
#define IPC_FLAG_DESTROY (1 << 6)
#define IPC_FLAG_CONSOLE_WRITE (1 << 7)
#define IPC_FLAG_NOTIFY_SIGNAL (1 << 8)
#define IPC_FLAG_NOTIFY_WAIT   (1 << 9)
#define IPC_FLAG_NOTIFY_POLL   (1 << 10)
#define IPC_FLAG_TIMER_ARM     (1 << 11)
#define IPC_FLAG_TIMER_DISARM  (1 << 12)
#define IPC_FLAG_REVOKE_FRAME  (1 << 13)
#define IPC_FLAG_EXIT (1 << 14)
#define IPC_FLAG_SPAWN (1 << 15)
#define IPC_FLAG_WAIT (1 << 16)
#define IPC_FLAG_CONSOLE_READ (1 << 17)
#define IPC_FLAG_PIPE_CREATE (1 << 18)
#define IPC_FLAG_PIPE_READ (1 << 19)
#define IPC_FLAG_PIPE_WRITE (1 << 20)
#define IPC_FLAG_PIPE_CLOSE (1 << 21)
#define IPC_FLAG_THREAD_INFO (1 << 22)
#define IPC_FLAG_SYS_INFO (1 << 23)
#define IPC_FLAG_FORK (1 << 24)
#define IPC_FLAG_SET_FSBASE (1 << 25)
#define IPC_FLAG_SELF_TID (1 << 26)
#define IPC_FLAG_EXEC (1 << 27)
#define IPC_FLAG_MOUNT (1 << 28)
#define IPC_FLAG_BOOTFS (1 << 29)
#define IPC_FLAG_BLK_IO (1 << 30)
// Bit 31 is the last one free in this 32-bit flags word. Reboot (merged in
// from a fork) rides in as IPC_FLAG_SYS_INFO's category 2 instead of a new
// bit, matching the r8-encoded sub-op pattern IPC_FLAG_SYS_INFO/
// IPC_FLAG_BLK_IO already use for exactly this situation.
#define IPC_ERR_NONE       0
#define IPC_ERR_NOT_FOUND -1
#define IPC_ERR_TIMEOUT   -2
#define IPC_ERR_CANCELED  -3
#define IPC_ERR_NO_CAP    -4
#define IPC_ERR_WOULDBLOCK -5
#define IPC_ERR_NO_MEM     -6
void sys_ipc(void);
void ipc_grant_console_writer(tid_t tid);
void ipc_grant_blk_owner(tid_t tid);
#endif
