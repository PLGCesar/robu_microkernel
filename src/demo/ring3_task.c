#include "robu/types.h"
#include "robu/arch.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/vm.h"
#include "../boot.h"

extern const uint8_t user_payload_start[];
extern const uint8_t user_payload_end[];

tid_t spawn_ring3_task(const char *name, uint8_t prio) {
    paddr_t as = vm_address_space_create();
    paddr_t code_frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t code_len = (uint64_t)(user_payload_end - user_payload_start);
    memcpy((void *)code_frame, user_payload_start, code_len);
    arch_vm_map_page(as, USER_CODE_VA, code_frame,
                     VM_PROT_READ | VM_PROT_EXEC | VM_PROT_USER);
    paddr_t stack_frame = pmm_alloc(PMM_COLOR_ANY);
    arch_vm_map_page(as, USER_STACK_VA, stack_frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    tcb_t *t = thread_create_user(name, USER_CODE_VA, USER_STACK_VA + PAGE_SIZE_4K,
                                  prio, as, PAGER_TID);
    return t->tid;
}

void ring3_task_demo_init(void) {
    spawn_ring3_task("ring3-A", 5);
    spawn_ring3_task("ring3-B", 5);
}
