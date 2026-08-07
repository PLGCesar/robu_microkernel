#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"
#define PVH_MAGIC 0x336ec578u
struct hvm_start_info {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t nr_modules;
    uint64_t modlist_paddr;
    uint64_t cmdline_paddr;
    uint64_t rsdp_paddr;
    uint64_t memmap_paddr;
    uint32_t memmap_entries;
    uint32_t reserved;
} __attribute__((packed));
struct hvm_memmap_table_entry {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));
struct hvm_modlist_entry {
    uint64_t paddr;
    uint64_t size;
    uint64_t cmdline_paddr;
    uint64_t reserved;
} __attribute__((packed));
#define HVM_MEMMAP_TYPE_RAM 1
extern uint64_t pvh_start_info_ptr;
#define FALLBACK_BASE 0x400000ULL
#define FALLBACK_LEN  0x1000000ULL
static const struct hvm_start_info *pvh_info(void) {
    const struct hvm_start_info *info =
        (const struct hvm_start_info *)pvh_start_info_ptr;
    if (pvh_start_info_ptr && info->magic == PVH_MAGIC && info->version >= 1) {
        return info;
    }
    return NULL;
}
int arch_boot_magic(uint32_t *out_magic) {
    const struct hvm_start_info *info = pvh_info();
    if (!info) {
        return -1;
    }
    *out_magic = info->magic;
    return 0;
}
const char *arch_boot_cmdline(void) {
    const struct hvm_start_info *info = pvh_info();
    if (!info || !info->cmdline_paddr) {
        return NULL;
    }
    return (const char *)info->cmdline_paddr;
}
int arch_boot_module(paddr_t *out_base, uint64_t *out_len) {
    const struct hvm_start_info *info = pvh_info();
    if (!info || info->nr_modules == 0 || !info->modlist_paddr) {
        return -1;
    }
    const struct hvm_modlist_entry *mods =
        (const struct hvm_modlist_entry *)info->modlist_paddr;
    *out_base = (paddr_t)mods[0].paddr;
    *out_len = mods[0].size;
    return 0;
}
void arch_detect_memory(paddr_t *out_base, uint64_t *out_len) {
    const struct hvm_start_info *info = pvh_info();
    if (info && info->memmap_paddr && info->memmap_entries > 0) {
        const struct hvm_memmap_table_entry *map =
            (const struct hvm_memmap_table_entry *)info->memmap_paddr;
        paddr_t best_base = 0;
        uint64_t best_len = 0;
        for (uint32_t i = 0; i < info->memmap_entries; i++) {
            if (map[i].type == HVM_MEMMAP_TYPE_RAM && map[i].size > best_len) {
                best_base = map[i].addr;
                best_len = map[i].size;
            }
        }
        if (best_len > 0) {
            kprintf("[mem] PVH memmap: %u entries, largest RAM region "
                    "[0x%lx-0x%lx)\n",
                    info->memmap_entries, best_base, best_base + best_len);
            *out_base = best_base;
            *out_len = best_len;
            return;
        }
    }
    kprintf("[mem] no usable PVH memmap; falling back to a conservative "
            "%lu KiB window\n",
            FALLBACK_LEN / 1024);
    *out_base = FALLBACK_BASE;
    *out_len = FALLBACK_LEN;
}
