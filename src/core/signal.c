#include "robu/types.h"
#include "robu/signal.h"
#include "robu/tcb.h"
#include "robu/sched.h"
#include "robu/spawn.h"
#include "robu/vm.h"
#include "robu/pmm.h"
#include "robu/arch.h"
#include "context.h"
extern paddr_t boot_pml4;
void sig_trampoline_init(void) {
    extern uint8_t sigtramp_start[];
    extern uint8_t sigtramp_end[];
    paddr_t frame = pmm_alloc(PMM_COLOR_ANY);
    memcpy((void *)frame, sigtramp_start, (uint64_t)(sigtramp_end - sigtramp_start));
    arch_vm_map_page((paddr_t)&boot_pml4, SIGTRAMPOLINE_VA, frame,
                      VM_PROT_READ | VM_PROT_USER | VM_PROT_EXEC);
}
int sig_send(tid_t target, int signum) {
    if (signum < 1 || signum >= ROBU_NSIG) {
        return -1;
    }
    tcb_t *t = sched_get_tcb(target);
    if (!t || t->state == THREAD_STATE_DEAD || t->state == THREAD_STATE_ZOMBIE) {
        return -1;
    }
    t->sig_pending |= (1ULL << (signum - 1));
    sched_wake(t);
    return 0;
}
static int sig_default_terminates(int signum) {
    switch (signum) {
    case SIGCHLD:
    case SIGURG:
    case SIGWINCH:
    case SIGCONT:
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
        return 0;
    default:
        return 1;
    }
}
void sig_try_deliver(struct tcb *t) {
    if (t->in_sig_handler) {
        return;
    }
    uint64_t deliverable = t->sig_pending & ~t->sig_mask;
    if (!deliverable) {
        return;
    }
    int signum = 1;
    while (!((deliverable >> (signum - 1)) & 1ULL)) {
        signum++;
    }
    t->sig_pending &= ~(1ULL << (signum - 1));
    sig_action_t *sa = &t->sig_actions[signum];
    if (sa->handler == ROBU_SIG_IGN) {
        return;
    }
    if (sa->handler == ROBU_SIG_DFL) {
        if (sig_default_terminates(signum)) {
            spawn_exit_current((int)(0x80000000u | (uint32_t)signum));
        }
        return;
    }
    t->saved_uctx_before_sig = t->uctx;
    t->saved_sig_mask = t->sig_mask;
    t->sig_mask |= sa->mask;
    if (!(sa->flags & SA_NODEFER)) {
        t->sig_mask |= (1ULL << (signum - 1));
    }
    t->in_sig_handler = 1;
    uint64_t handler = sa->handler;
    if (sa->flags & SA_RESETHAND) {
        sa->handler = ROBU_SIG_DFL;
    }
    uint64_t new_rsp = (t->uctx.rsp - 256) & ~0xFULL;
    new_rsp -= 8;
    *(uint64_t *)new_rsp = SIGTRAMPOLINE_VA;
    t->uctx.rsp = new_rsp;
    t->uctx.rip = handler;
    t->uctx.rdi = (uint64_t)signum;
}
