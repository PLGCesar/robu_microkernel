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

#define ROOTFS_MAX_SIZE (1536 * 1024)

static uint8_t rootfs_buf[ROOTFS_MAX_SIZE];
static uint64_t rootfs_len;

const uint8_t *service_elf_start, *service_elf_end;
const uint8_t *task_elf_start, *task_elf_end;

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

static const char *rootfs_split_pair(char *buf, const char **second) {
    for (char *p = buf; *p; p++) {
        if (*p == ',') {
            *p = '\0';
            *second = p + 1;
            return buf;
        }
    }
    *second = "";
    return buf;
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

    static char apps_buf[128];
    const char *apps = cmdline_get("apps");
    if (!apps) {
        apps = "hello_service,hello_task";
    }
    size_t len = strlen(apps);
    if (len >= sizeof(apps_buf)) {
        len = sizeof(apps_buf) - 1;
    }
    memcpy(apps_buf, apps, len);
    apps_buf[len] = '\0';
    const char *task_name;
    const char *service_name = rootfs_split_pair(apps_buf, &task_name);

    if (rootfs_lookup(service_name, &service_elf_start, &service_elf_end) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", service_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    if (rootfs_lookup(task_name, &task_elf_start, &task_elf_end) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", task_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    kprintf("[boot] loaded service='%s' task='%s'\n", service_name, task_name);
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
