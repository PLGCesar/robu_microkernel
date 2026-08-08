#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"
#include "robu/sched.h"
#include "robu/elf.h"
#include "robu/cmdline.h"
#include "robu/tar.h"
#include "robu/captable.h"
#include "robu/ramfs.h"
#include "robu/rootfs.h"
#include "../boot.h"

// mlibc-linked binaries are ~100x bigger than the old apps/libc ones (each
// statically links libc.a/libm.a in full, no dynamic linking), so migrating
// the rootfs to them needs a much bigger buffer than apps/libc ever did.
#define ROOTFS_MAX_SIZE (24 * 1024 * 1024)

static uint8_t rootfs_buf[ROOTFS_MAX_SIZE];
static uint64_t rootfs_len;

int rootfs_lookup(const char *name, const uint8_t **out_start, const uint8_t **out_end) {
    const uint8_t *start;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, name, &start, &sz) != 0) {
        return -1;
    }
    *out_start = start;
    *out_end = start + sz;
    return 0;
}

void rootfs_init(void) {
    cmdline_parse(arch_boot_cmdline());
    paddr_t mod_base;
    uint64_t mod_len;
    if (arch_boot_module(&mod_base, &mod_len) != 0) {
        kprintf("[boot] FATAL: no boot module supplied -- pass -initrd <rootfs.tar>\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    if (mod_len > sizeof(rootfs_buf)) {
        kprintf("[boot] FATAL: rootfs module is %lu bytes, exceeds the %lu byte buffer\n",
                mod_len, (uint64_t)sizeof(rootfs_buf));
        for (;;) { asm volatile("cli; hlt"); }
    }
    memcpy(rootfs_buf, (const void *)mod_base, mod_len);
    rootfs_len = mod_len;
    kprintf("[boot] rootfs: %lu bytes copied from module at 0x%lx\n", mod_len, (uint64_t)mod_base);
}

void root_task_init(paddr_t untyped_base, uint64_t untyped_size) {
    const char *root_name = cmdline_get("root");
    if (!root_name) {
        root_name = "root_task";
    }
    const uint8_t *root_elf_start, *root_elf_end;
    if (rootfs_lookup(root_name, &root_elf_start, &root_elf_end) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", root_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    tcb_t *root = elf_load_and_spawn("root-task", root_elf_start, root_elf_end, 13, PAGER_TID);
    if (!root) {
        kprintf("[boot] FATAL: root task failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kcap_grant(root->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, untyped_size);
    kprintf("[boot] root task: tid=%u\n", root->tid);
}

void ramfs_bin_seed_init(void) {
    static const char *const names[] = {
        "sh", "ls", "cat", "touch", "tail", "file", "find", "cp", "mv"
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        const uint8_t *start, *end;
        if (rootfs_lookup(names[i], &start, &end) != 0) {
            kprintf("[boot] /bin seed: FATAL: rootfs has no entry named '%s'\n", names[i]);
            for (;;) { asm volatile("cli; hlt"); }
        }
        uint64_t sz = (uint64_t)(end - start);

        char path[RAMFS_NAME_MAX];
        uint32_t p = 0;
        static const char prefix[] = "bin/";
        for (uint32_t j = 0; prefix[j] && p < sizeof(path) - 1; j++) {
            path[p++] = prefix[j];
        }
        for (uint32_t j = 0; names[i][j] && p < sizeof(path) - 1; j++) {
            path[p++] = names[i][j];
        }
        path[p] = '\0';

        int64_t h = ramfs_open(path, RAMFS_O_CREAT | RAMFS_O_TRUNC);
        if (h < 0) {
            kprintf("[boot] /bin seed: FATAL: ramfs_open('%s') failed rc=%ld\n", path, h);
            for (;;) { asm volatile("cli; hlt"); }
        }
        uint64_t off = 0;
        while (off < sz) {
            uint64_t chunk = sz - off;
            if (chunk > RAMFS_WRITE_MAX) {
                chunk = RAMFS_WRITE_MAX;
            }
            int64_t n = ramfs_write((uint64_t)h, start + off, chunk);
            if (n <= 0) {
                kprintf("[boot] /bin seed: FATAL: ramfs_write('%s') failed at off=%lu\n",
                        path, off);
                for (;;) { asm volatile("cli; hlt"); }
            }
            off += (uint64_t)n;
        }
        ramfs_close((uint64_t)h);
    }
    kprintf("[boot] /bin seed: %lu real binaries copied into ramfs\n",
            (uint64_t)(sizeof(names) / sizeof(names[0])));
}

void ramfs_etc_seed_init(void) {
    const uint8_t *start, *end;
    if (rootfs_lookup("rc.conf", &start, &end) != 0) {
        kprintf("[boot] /etc seed: no 'rc.conf' entry in rootfs, skipping\n");
        return;
    }
    uint64_t sz = (uint64_t)(end - start);

    int64_t h = ramfs_open("etc/rc.conf", RAMFS_O_CREAT | RAMFS_O_TRUNC);
    if (h < 0) {
        kprintf("[boot] /etc seed: ramfs_open('etc/rc.conf') failed rc=%ld\n", h);
        return;
    }
    uint64_t off = 0;
    while (off < sz) {
        uint64_t chunk = sz - off;
        if (chunk > RAMFS_WRITE_MAX) {
            chunk = RAMFS_WRITE_MAX;
        }
        int64_t n = ramfs_write((uint64_t)h, start + off, chunk);
        if (n <= 0) {
            kprintf("[boot] /etc seed: ramfs_write('etc/rc.conf') failed at off=%lu\n", off);
            ramfs_close((uint64_t)h);
            return;
        }
        off += (uint64_t)n;
    }
    ramfs_close((uint64_t)h);
    kprintf("[boot] /etc seed: rc.conf (%lu bytes) copied into ramfs\n", sz);
}
