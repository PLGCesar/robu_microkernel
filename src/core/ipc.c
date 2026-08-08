#include "robu/ipc.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/captable.h"
#include "robu/trace.h"
#include "robu/kprintf.h"
#include "robu/spawn.h"
#include "robu/pipe.h"
#include "robu/pmm.h"
static tid_t console_writer_tid = 0;
void ipc_grant_console_writer(tid_t tid) {
    console_writer_tid = tid;
}
static void payload_from_frame(msg_regs_t *m, const arch_uctx_t *f) {
    m->word[0] = f->r8;
    m->word[1] = f->r9;
    m->word[2] = f->r10;
    m->word[3] = f->r12;
    m->word[4] = f->r13;
    m->word[5] = f->r14;
}
static void payload_to_frame(arch_uctx_t *f, const msg_regs_t *m) {
    f->r8 = m->word[0];
    f->r9 = m->word[1];
    f->r10 = m->word[2];
    f->r12 = m->word[3];
    f->r13 = m->word[4];
    f->r14 = m->word[5];
}
static void sendq_append(tcb_t *rcv, tcb_t *s) {
    s->sq_next = NULL;
    if (rcv->send_q_tail) {
        rcv->send_q_tail->sq_next = s;
    } else {
        rcv->send_q_head = s;
    }
    rcv->send_q_tail = s;
}
static tcb_t *sendq_take(tcb_t *rcv, tid_t filter) {
    tcb_t *prev = NULL;
    for (tcb_t *s = rcv->send_q_head; s; prev = s, s = s->sq_next) {
        if (filter != IPC_TID_ANY && s->tid != filter) {
            continue;
        }
        if (prev) {
            prev->sq_next = s->sq_next;
        } else {
            rcv->send_q_head = s->sq_next;
        }
        if (rcv->send_q_tail == s) {
            rcv->send_q_tail = prev;
        }
        s->sq_next = NULL;
        return s;
    }
    return NULL;
}
static void deliver_to_frame(tcb_t *rcv, tid_t from, const msg_regs_t *m) {
    arch_uctx_t *f = &rcv->uctx;
    payload_to_frame(f, m);
    f->rsi = from;
    f->rax = (uint64_t)IPC_ERR_NONE;
}
static void complete_parked_sender(tcb_t *cur, arch_uctx_t *f, tcb_t *s) {
    int xfer_rc = 0;
    if (s->ipc_flags & IPC_FLAG_MAP) {
        xfer_rc = vm_batch_map(cur->address_space, s->tid, (vm_map_msg_t *)&s->ipc_buffer);
    } else if (s->ipc_flags & IPC_FLAG_XFER) {
        xfer_rc = vm_xfer_pages(s->address_space, cur->address_space,
                                (vm_xfer_msg_t *)&s->ipc_buffer);
    } else if (s->ipc_flags & IPC_FLAG_SHARE) {
        xfer_rc = vm_share_pages(s->address_space, cur->address_space,
                                 (vm_xfer_msg_t *)&s->ipc_buffer);
    }
    payload_to_frame(f, &s->ipc_buffer);
    f->rsi = s->tid;
    f->rax = (uint64_t)IPC_ERR_NONE;
    s->uctx.rax = (uint64_t)(xfer_rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
    sched_stats.ipc_msgs++;
    TRACE(TRACE_EVT_IPC_RECV, cur->tid, s->tid, s->ipc_flags, 0, 0, 0);
    if (s->ipc_recv_after != (tid_t)~0u) {
        s->ipc_partner = s->ipc_recv_after;
        s->ipc_recv_after = (tid_t)~0u;
        s->state = THREAD_STATE_WAIT_RECV;
    } else {
        sched_wake(s);
    }
}
void sys_ipc(void) {
    tcb_t *cur = current_thread;
    arch_uctx_t *f = &cur->uctx;
    tid_t dest_tid = (tid_t)f->rdi;
    tid_t src_filter = (tid_t)f->rsi;
    uint32_t flags = (uint32_t)f->rdx;
    int want_recv = flags & IPC_FLAG_RECV;
    if (dest_tid == 0 && !want_recv) {
        if (flags & IPC_FLAG_RETYPE) {
            uint64_t out_slot = 0, out_addr = 0;
            int rc = cap_retype(cur->tid, (uint32_t)f->r8, (uint32_t)f->r9,
                                 f->r10, f->r12, f->r13, f->r14, &out_slot, &out_addr);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            f->r8 = out_slot;
            f->r9 = out_addr;
            return;
        }
        if (flags & IPC_FLAG_DESTROY) {
            int rc = cap_destroy(cur->tid, (uint32_t)f->r8);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            return;
        }
        if (flags & IPC_FLAG_CONSOLE_WRITE) {
            if (cur->tid != console_writer_tid) {
                f->rax = (uint64_t)IPC_ERR_NO_CAP;
                return;
            }
            uint64_t len = f->r8;
            if (len > 40) {
                len = 40;
            }
            uint64_t words[5] = { f->r9, f->r10, f->r12, f->r13, f->r14 };
            kwrite((const uint8_t *)words, len);
            f->rax = (uint64_t)IPC_ERR_NONE;
            return;
        }
        if (flags & IPC_FLAG_CONSOLE_READ) {
            if (cur->tid != console_writer_tid) {
                f->rax = (uint64_t)IPC_ERR_NO_CAP;
                return;
            }
            uint64_t words[5] = {0, 0, 0, 0, 0};
            int n = arch_console_read_line_bytes((uint8_t *)words, 40);
            f->r8 = (uint64_t)n;
            f->r9 = words[0];
            f->r10 = words[1];
            f->r12 = words[2];
            f->r13 = words[3];
            f->r14 = words[4];
            f->rax = (uint64_t)IPC_ERR_NONE;
            return;
        }
        if (flags & IPC_FLAG_NOTIFY_SIGNAL) {
            int rc = cap_notif_signal(cur->tid, (uint32_t)f->r8, f->r9);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            return;
        }
        if (flags & IPC_FLAG_NOTIFY_WAIT) {
            uint64_t bits = 0;
            int rc = cap_notif_wait_begin(cur->tid, (uint32_t)f->r8, &bits);
            if (rc < 0) {
                f->rax = (uint64_t)IPC_ERR_NO_CAP;
                return;
            }
            if (rc == 1) {
                f->rax = (uint64_t)IPC_ERR_NONE;
                f->r8 = bits;
                return;
            }
            sched_block(THREAD_STATE_WAIT_NOTIFICATION);
            return;
        }
        if (flags & IPC_FLAG_NOTIFY_POLL) {
            uint64_t bits = 0;
            int rc = cap_notif_poll(cur->tid, (uint32_t)f->r8, &bits);
            if (rc == -1) {
                f->rax = (uint64_t)IPC_ERR_NO_CAP;
                return;
            }
            if (rc == -2) {
                f->rax = (uint64_t)IPC_ERR_WOULDBLOCK;
                return;
            }
            f->rax = (uint64_t)IPC_ERR_NONE;
            f->r8 = bits;
            return;
        }
        if (flags & IPC_FLAG_TIMER_ARM) {
            int rc = cap_timer_arm(cur->tid, (uint32_t)f->r8, (uint32_t)f->r9,
                                   f->r10, f->r12, f->r13);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            return;
        }
        if (flags & IPC_FLAG_TIMER_DISARM) {
            int rc = cap_timer_disarm(cur->tid, (uint32_t)f->r8);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            return;
        }
        if (flags & IPC_FLAG_REVOKE_FRAME) {
            int rc = cap_revoke_frame(cur->tid, (uint32_t)f->r8);
            f->rax = (uint64_t)(rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            return;
        }
        if (flags & IPC_FLAG_EXIT) {
            if (cur->address_space == 0) {
                f->rax = (uint64_t)IPC_ERR_NO_CAP;
                return;
            }
            spawn_exit_current((int)(f->r8 & 0xFF));
            return;
        }
        if (flags & IPC_FLAG_SPAWN) {
            tid_t child_tid = 0;
            int rc = spawn_create(cur, f->r8, f->r9, &child_tid);
            f->rax = (uint64_t)rc;
            f->r8 = child_tid;
            return;
        }
        if (flags & IPC_FLAG_WAIT) {
            tid_t filter = (tid_t)f->r8;
            tid_t reaped_tid;
            int status;
            int rc = spawn_wait_begin(cur, filter, &reaped_tid, &status);
            if (rc == 1) {
                f->rax = (uint64_t)IPC_ERR_NONE;
                f->r8 = reaped_tid;
                f->r9 = (uint64_t)(int64_t)status;
                return;
            }
            if (rc == -1) {
                f->rax = (uint64_t)IPC_ERR_NOT_FOUND;
                return;
            }
            if (flags & IPC_FLAG_NOBLOCK) {
                f->rax = (uint64_t)IPC_ERR_WOULDBLOCK;
                return;
            }
            cur->wait_filter = filter;
            sched_block(THREAD_STATE_WAIT_CHILD);
            return;
        }
        if (flags & IPC_FLAG_PIPE_CREATE) {
            uint64_t handle = 0;
            int64_t rc = pipe_create(cur->tid, &handle);
            f->rax = (uint64_t)rc;
            f->r8 = handle;
            return;
        }
        if (flags & IPC_FLAG_PIPE_READ) {
            uint64_t out_len = 0;
            uint8_t buf[PIPE_CHUNK_MAX];
            int64_t rc = pipe_read(f->r8, buf, f->r9, &out_len);
            f->rax = (uint64_t)rc;
            f->r8 = out_len;
            uint64_t words[4] = {0, 0, 0, 0};
            for (uint64_t i = 0; i < out_len; i++) {
                ((uint8_t *)words)[i] = buf[i];
            }
            f->r10 = words[0];
            f->r12 = words[1];
            f->r13 = words[2];
            f->r14 = words[3];
            return;
        }
        if (flags & IPC_FLAG_PIPE_WRITE) {
            uint64_t len = f->r9 > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : f->r9;
            uint64_t words[4] = { f->r10, f->r12, f->r13, f->r14 };
            uint64_t out_len = 0;
            int64_t rc = pipe_write(f->r8, (const uint8_t *)words, len, &out_len);
            f->rax = (uint64_t)rc;
            f->r8 = out_len;
            return;
        }
        if (flags & IPC_FLAG_PIPE_CLOSE) {
            int64_t rc = pipe_close(cur->tid, f->r8, (int)f->r9);
            f->rax = (uint64_t)rc;
            return;
        }
        if (flags & IPC_FLAG_FORK) {
            tid_t child_tid = 0;
            int rc = spawn_fork_create(cur, &child_tid);
            f->rax = (uint64_t)rc;
            f->r8 = child_tid;
            return;
        }
        if (flags & IPC_FLAG_SET_FSBASE) {
            cur->fs_base = f->r8;
            arch_set_fsbase(cur->fs_base);
            f->rax = (uint64_t)IPC_ERR_NONE;
            return;
        }
        if (flags & IPC_FLAG_THREAD_INFO) {
            thread_state_t state;
            uint8_t prio;
            int32_t exit_status;
            tid_t parent_tid;
            const char *name;
            if (sched_thread_info((tid_t)f->r8, &state, &prio, &exit_status,
                                  &parent_tid, &name) != 0) {
                f->rax = (uint64_t)IPC_ERR_NOT_FOUND;
                return;
            }
            uint64_t w13 = 0, w14 = 0;
            int ni = 0;
            for (; ni < 8 && name[ni]; ni++) {
                w13 |= ((uint64_t)(uint8_t)name[ni]) << (8 * ni);
            }
            if (ni == 8) {
                for (int nj = 0; nj < 8 && name[8 + nj]; nj++) {
                    w14 |= ((uint64_t)(uint8_t)name[8 + nj]) << (8 * nj);
                }
            }
            f->rax = (uint64_t)IPC_ERR_NONE;
            f->r8 = (uint64_t)state;
            f->r9 = (uint64_t)prio;
            f->r10 = (uint64_t)(int64_t)exit_status;
            f->r12 = (uint64_t)parent_tid;
            f->r13 = w13;
            f->r14 = w14;
            return;
        }
        if (flags & IPC_FLAG_SYS_INFO) {
            uint64_t category = f->r8;
            if (category == 0) {
                f->rax = (uint64_t)IPC_ERR_NONE;
                f->r8 = pmm_stats.total_frames;
                f->r9 = pmm_stats.free_frames;
                f->r10 = pmm_stats.alloc_calls;
                f->r12 = pmm_stats.free_calls;
            } else if (category == 1) {
                f->rax = (uint64_t)IPC_ERR_NONE;
                f->r8 = sched_stats.full_scheds;
                f->r9 = sched_stats.preempts;
                f->r10 = sched_stats.ipc_msgs;
                f->r12 = sched_stats.ticks;
                f->r13 = sched_stats.timer_traps;
                f->r14 = sched_stats.kicks_sent;
            } else {
                f->rax = (uint64_t)IPC_ERR_NOT_FOUND;
            }
            return;
        }
        f->rax = (uint64_t)IPC_ERR_NONE;
        if (src_filter == 0) {
            sched_yield();
        } else {
            sched_sleep(src_filter);
        }
        return;
    }
    if (dest_tid != 0) {
        TRACE(TRACE_EVT_IPC_SEND, dest_tid, cur->tid, flags, 0, 0, 0);
        tcb_t *dest = sched_get_tcb(dest_tid);
        if (!dest || dest == cur) {
            f->rax = (uint64_t)IPC_ERR_NOT_FOUND;
            return;
        }
        if (dest->state == THREAD_STATE_WAIT_PAGEFAULT &&
            cur->tid == dest->pager_tid) {
            sched_wake(dest);
            f->rax = (uint64_t)IPC_ERR_NONE;
            if (!want_recv) {
                return;
            }
        } else if (dest->state == THREAD_STATE_WAIT_RECV &&
                   (dest->ipc_partner == IPC_TID_ANY ||
                    dest->ipc_partner == cur->tid)) {
            msg_regs_t m;
            payload_from_frame(&m, f);
            int xfer_rc = 0;
            if (flags & IPC_FLAG_MAP) {
                xfer_rc = vm_batch_map(dest->address_space, cur->tid, (vm_map_msg_t *)&m);
            } else if (flags & IPC_FLAG_XFER) {
                xfer_rc = vm_xfer_pages(cur->address_space, dest->address_space,
                                        (vm_xfer_msg_t *)&m);
            } else if (flags & IPC_FLAG_SHARE) {
                xfer_rc = vm_share_pages(cur->address_space, dest->address_space,
                                         (vm_xfer_msg_t *)&m);
            }
            deliver_to_frame(dest, cur->tid, &m);
            sched_stats.ipc_msgs++;
            TRACE(TRACE_EVT_IPC_RECV, dest->tid, cur->tid, flags, 0, 0, 0);
            f->rax = (uint64_t)(xfer_rc == 0 ? IPC_ERR_NONE : IPC_ERR_NO_CAP);
            if (want_recv) {
                tcb_t *waiting = sendq_take(cur, src_filter);
                if (waiting) {
                    sched_wake(dest);
                    complete_parked_sender(cur, f, waiting);
                    return;
                }
                cur->ipc_partner = src_filter;
                sched_block(THREAD_STATE_WAIT_RECV);
                dest->timeslice_left = cur->timeslice_left;
                sched_direct_switch(dest);
            } else if (dest->prio >= cur->prio) {
                sched_ready_now(cur);
                dest->timeslice_left = cur->timeslice_left;
                sched_direct_switch(dest);
            } else {
                sched_wake(dest);
            }
            return;
        } else {
            payload_from_frame(&cur->ipc_buffer, f);
            cur->ipc_flags = flags;
            cur->ipc_partner = dest_tid;
            cur->ipc_recv_after = want_recv ? src_filter : (tid_t)~0u;
            sendq_append(dest, cur);
            sched_block(THREAD_STATE_WAIT_SEND);
            return;
        }
    }
    {
        tcb_t *s = sendq_take(cur, src_filter);
        if (s) {
            complete_parked_sender(cur, f, s);
            return;
        }
        vm_fault_msg_t fault;
        if (vm_take_queued_fault(cur, &fault)) {
            f->r8 = fault.fault_address;
            f->r9 = fault.error_code;
            f->r10 = fault.faulting_thread;
            f->rsi = fault.faulting_thread;
            f->rax = (uint64_t)IPC_ERR_NONE;
            return;
        }
        if (flags & IPC_FLAG_NOBLOCK) {
            f->rax = (uint64_t)IPC_ERR_WOULDBLOCK;
            return;
        }
        cur->ipc_partner = src_filter;
        sched_block(THREAD_STATE_WAIT_RECV);
    }
}
