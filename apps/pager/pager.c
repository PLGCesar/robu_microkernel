#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ipc.h"
#include "robu/vm.h"
#include "robu/pmm.h"
// The kernel's page-fault trap handler (vm_handle_page_fault(), src/core/
// vm.c) delivers a fault to this process exactly like any other blocked
// thread getting resumed -- it pokes this process's saved r8/r9/r10 with
// {fault_addr, error_code, faulting_tid} and switches straight to it, so an
// ordinary ipc_recv() picks it up with zero special-casing.
//
// This process must never take a page fault of its own after it spawns --
// its own fault would need to be delivered to its own pager_tid, which is
// itself, while it isn't blocked in ipc_recv(). That's an unrecoverable
// deadlock. It's spawned first (tid 1, same as the kernel-mode pager it
// replaces) with a fully eager-mapped ELF image (src/core/elf.c maps every
// page up to p_memsz at spawn time), and this file uses only fixed static
// storage and no dynamic growth, so it structurally cannot fault.
void _start(void) {
    msg_regs_t m;
    tid_t from;
    int color_cursor = 0;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        vaddr_t fault_addr = m.word[0];
        tid_t faulter = (tid_t)m.word[2];
        vaddr_t page_va = fault_addr & ~(PAGE_SIZE_4K - 1);
        int color = color_cursor++ & (PMM_NUM_COLORS - 1);
        msg_regs_t resolve = (msg_regs_t){0};
        resolve.word[0] = faulter;
        resolve.word[1] = page_va;
        resolve.word[2] = (uint64_t)color;
        resolve.word[3] = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER;
        robu_ipc_raw(0, 0, IPC_FLAG_RESOLVE_FAULT, &resolve, NULL);
        ipc_send(faulter, NULL);
    }
}
