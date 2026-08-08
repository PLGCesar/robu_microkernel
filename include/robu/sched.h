#ifndef ROBU_SCHED_H
#define ROBU_SCHED_H
#include "robu/tcb.h"
#define SCHED_HZ               100
#define SCHED_PRIO_LEVELS      32
#define SCHED_TIMESLICE_TICKS  5
#define SCHED_MAX_THREADS      64
#define STACK_SIZE 8192
typedef struct {
    uint64_t direct_switches;
    uint64_t full_scheds;
    uint64_t preempts;
    uint64_t ipc_msgs;
    uint64_t ticks;
    uint64_t affinity_hits;
    uint64_t affinity_misses;
    uint64_t timer_traps;
    uint64_t kicks_sent;
} sched_stats_t;
extern sched_stats_t sched_stats;
void sched_init(void);
void sched_init_ap(void);
tcb_t *thread_create(const char *name, void (*entry)(void), void *stack_top, uint8_t prio);
tcb_t *thread_create_user(const char *name, vaddr_t entry, vaddr_t user_stack_top,
                          uint8_t prio, paddr_t address_space, tid_t pager_tid);
tcb_t *thread_create_user_locked(const char *name, vaddr_t entry, vaddr_t user_stack_top,
                                 uint8_t prio, paddr_t address_space, tid_t pager_tid);
tcb_t *thread_create_user_argv(const char *name, vaddr_t entry, vaddr_t user_stack_top,
                               uint8_t prio, paddr_t address_space, tid_t pager_tid,
                               uint64_t argc, uint64_t argv, uint64_t envp, uint64_t heap_base,
                               uint64_t spawn_info);
tcb_t *thread_create_user_argv_locked(const char *name, vaddr_t entry, vaddr_t user_stack_top,
                                      uint8_t prio, paddr_t address_space, tid_t pager_tid,
                                      uint64_t argc, uint64_t argv, uint64_t envp,
                                      uint64_t heap_base, uint64_t spawn_info);
tcb_t *thread_create_forked_locked(const char *name, paddr_t address_space, tid_t pager_tid,
                                   uint8_t prio, const tcb_t *parent);
tcb_t *sched_get_tcb(tid_t tid);
uint64_t sched_now(void);
int sched_thread_info(tid_t tid, thread_state_t *state_out, uint8_t *prio_out,
                      int32_t *exit_status_out, tid_t *parent_tid_out,
                      const char **name_out);
tcb_t *sched_find_zombie_child(tid_t parent, tid_t filter, int *out_has_live);
void sched_reap_zombie(tcb_t *t);
void sched_start(void) __attribute__((noreturn));
void sched_join_ap(void) __attribute__((noreturn));
void sched_resume(void) __attribute__((noreturn));
void sched_wake(tcb_t *t);
void sched_ready_now(tcb_t *t);
void sched_direct_switch(tcb_t *dest);
void sched_block(thread_state_t why);
void sched_terminate_current(void);
void sched_terminate_to(thread_state_t final_state);
int sched_terminate(tid_t victim_tid);
void sched_yield(void);
void sched_sleep(uint64_t ticks);
void sched_note_deadline(uint64_t tick);
void sched_request_resched(void);
void sched_tick(void);
void sched_lock_acquire(void);
void sched_lock_release(void);
#endif
