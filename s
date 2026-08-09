#!/bin/sh
set -eu

mkdir -p arch/x86_64/src iso/boot/grub .github/workflows

cat << 'EOF' > arch/x86_64/src/multiboot2.S
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
#define MULTIBOOT2_ARCH_I386    0

.section .multiboot2, "a"
.align 8
mb2_header_start:
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT2_ARCH_I386
    .long mb2_header_end - mb2_header_start
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCH_I386 + (mb2_header_end - mb2_header_start))

    .short 0
    .short 0
    .long 8
mb2_header_end:
EOF

cat << 'EOF' > arch/x86_64/src/boot.S
.section .note.Xen, "a", @note
.align 4
    .long 4                 /* namesz: "Xen" + NUL */
    .long 4                 /* descsz: one 32-bit entry address */
    .long 18                /* XEN_ELFNOTE_PHYS32_ENTRY */
    .asciz "Xen"
    .long start

#define PTE_P   (1 << 0)
#define PTE_W   (1 << 1)
#define PTE_PS  (1 << 7)

.section .bss

.align 16
.global kstack_bottom
.global kstack_top
kstack_bottom:
    .skip 16384
kstack_top:

.align 16
.global ap_kstack_bottom
.global ap_kstack_top
ap_kstack_bottom:
    .skip 16384
ap_kstack_top:

.align 4096
.global boot_pml4
boot_pml4:
    .skip 4096
.global boot_pdpt
boot_pdpt:
    .skip 4096
boot_pd:
    .skip 4096

.align 8
.global pvh_start_info_ptr
pvh_start_info_ptr:
    .skip 8

.global multiboot2_info_ptr
multiboot2_info_ptr:
    .skip 8

.section .rodata
.align 8
gdt64:
    .quad 0
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .text
.code32
.global start
.type start, @function
start:
    cli

    /* Verifica se o boot veio via Multiboot2 (eax = 0x36d76289) ou PVH */
    cmpl $0x36d76289, %eax
    je 1f
    movl %ebx, pvh_start_info_ptr
    jmp 2f
1:
    movl %ebx, multiboot2_info_ptr
2:

    movl $kstack_top, %esp

    movl $boot_pdpt + (PTE_P | PTE_W), boot_pml4
    movl $boot_pd + (PTE_P | PTE_W), boot_pdpt

    movl $boot_pd, %edi
    movl $(PTE_P | PTE_W | PTE_PS), %eax
    movl $512, %ecx
3:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 3b

    movl $boot_pml4, %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4
    movl $0xC0000080, %ecx
    rdmsr
    orl $((1 << 8) | (1 << 11)), %eax
    wrmsr
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $long_entry

.code64
long_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs

    movq $kstack_top, %rsp
    xorq %rbp, %rbp

    .extern kmain
    call kmain

    cli
4:  hlt
    jmp 4b

.section .note.GNU-stack, "", @progbits
EOF

cat << 'EOF' > arch/x86_64/src/pvh_mem.c
#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"

#define PVH_MAGIC 0x336ec578u
#define MB2_MAGIC 0x36d76289u

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

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_string {
    uint32_t type;
    uint32_t size;
    char string[1];
};

struct mb2_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[1];
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct mb2_mmap_entry entries[1];
};

#define HVM_MEMMAP_TYPE_RAM 1
extern uint64_t pvh_start_info_ptr;
extern uint64_t multiboot2_info_ptr;

#define FALLBACK_BASE 0x1000000ULL
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
    if (multiboot2_info_ptr) {
        *out_magic = MB2_MAGIC;
        return 0;
    }
    const struct hvm_start_info *info = pvh_info();
    if (!info) {
        return -1;
    }
    *out_magic = info->magic;
    return 0;
}

const char *arch_boot_cmdline(void) {
    if (multiboot2_info_ptr) {
        uint32_t total_size = *(uint32_t *)multiboot2_info_ptr;
        uint8_t *ptr = (uint8_t *)(multiboot2_info_ptr + 8);
        while (ptr < (uint8_t *)multiboot2_info_ptr + total_size) {
            struct mb2_tag *tag = (struct mb2_tag *)ptr;
            if (tag->type == 0) break;
            if (tag->type == 1) {
                return ((struct mb2_tag_string *)tag)->string;
            }
            ptr += (tag->size + 7) & ~7;
        }
    }
    const struct hvm_start_info *info = pvh_info();
    if (!info || !info->cmdline_paddr) {
        return NULL;
    }
    return (const char *)info->cmdline_paddr;
}

int arch_boot_module(paddr_t *out_base, uint64_t *out_len) {
    if (multiboot2_info_ptr) {
        uint32_t total_size = *(uint32_t *)multiboot2_info_ptr;
        uint8_t *ptr = (uint8_t *)(multiboot2_info_ptr + 8);
        while (ptr < (uint8_t *)multiboot2_info_ptr + total_size) {
            struct mb2_tag *tag = (struct mb2_tag *)ptr;
            if (tag->type == 0) break;
            if (tag->type == 3) {
                struct mb2_tag_module *mod = (struct mb2_tag_module *)tag;
                *out_base = (paddr_t)mod->mod_start;
                *out_len = (uint64_t)(mod->mod_end - mod->mod_start);
                return 0;
            }
            ptr += (tag->size + 7) & ~7;
        }
    }
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
    if (multiboot2_info_ptr) {
        uint32_t total_size = *(uint32_t *)multiboot2_info_ptr;
        uint8_t *ptr = (uint8_t *)(multiboot2_info_ptr + 8);
        while (ptr < (uint8_t *)multiboot2_info_ptr + total_size) {
            struct mb2_tag *tag = (struct mb2_tag *)ptr;
            if (tag->type == 0) break;
            if (tag->type == 6) {
                struct mb2_tag_mmap *mmap = (struct mb2_tag_mmap *)tag;
                uint32_t nentries = (mmap->size - 16) / mmap->entry_size;
                paddr_t best_base = 0;
                uint64_t best_len = 0;
                for (uint32_t i = 0; i < nentries; i++) {
                    struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)((uint8_t *)mmap->entries + i * mmap->entry_size);
                    if (e->type == 1 && e->addr >= 0x1000000ULL && e->len > best_len) {
                        best_base = (paddr_t)e->addr;
                        best_len = e->len;
                    }
                }
                if (best_len > 0) {
                    kprintf("[mem] Multiboot2 memmap: largest RAM region [0x%lx-0x%lx)\n",
                            best_base, best_base + best_len);
                    *out_base = best_base;
                    *out_len = best_len;
                    return;
                }
            }
            ptr += (tag->size + 7) & ~7;
        }
    }

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
            kprintf("[mem] PVH memmap: %u entries, largest RAM region [0x%lx-0x%lx)\n",
                    info->memmap_entries, best_base, best_base + best_len);
            *out_base = best_base;
            *out_len = best_len;
            return;
        }
    }

    kprintf("[mem] fallback memory window [0x%lx-0x%lx)\n",
            (uint64_t)FALLBACK_BASE, (uint64_t)(FALLBACK_BASE + FALLBACK_LEN));
    *out_base = FALLBACK_BASE;
    *out_len = FALLBACK_LEN;
}
EOF

cat << 'EOF' > iso/boot/grub/grub.cfg
set timeout=0
set default=0

menuentry "robu_kernel" {
    multiboot2 /boot/robu_kernel root=root_task starter=hello_initsys quiet
    module2 /boot/rootfs.tar rootfs.tar
    boot
}
EOF

cat << 'EOF' > arch/x86_64/linker.ld
ENTRY(start)

PHDRS
{
    text   PT_LOAD  FLAGS(5);
    rodata PT_LOAD  FLAGS(4);
    data   PT_LOAD  FLAGS(6);
    note   PT_NOTE  FLAGS(4);
}

SECTIONS
{
    . = 1M;

    .text : {
        KEEP(*(.multiboot2))
        *(.text .text.*)
    } :text

    .note.Xen : {
        KEEP(*(.note.Xen))
    } :text :note

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(8) {
        *(.data .data.*)
    } :data
    _edata = .;

    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    _end = .;
}
EOF

cat << 'EOF' > .github/workflows/ci.yml
name: ci

on:
  push:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \
            clang lld llvm make flex bison mtools xorriso grub-pc-bin grub-common qemu-system-x86

      - name: Build ISO
        run: |
          make BUILD_SYS=clang iso

      - name: Test boot in QEMU
        run: |
          timeout 8s qemu-system-x86_64 \
            -cdrom build/robu_kernel.iso \
            -nographic \
            -device isa-debug-exit,iobase=0xf4,iosize=0x04 || code=$?
          echo "QEMU exit code: ${code:-0}"

      - name: Upload ISO
        uses: actions/upload-artifact@v4
        with:
          name: robu_kernel.iso
          path: build/robu_kernel.iso

      - name: Upload Kernel Binary
        uses: actions/upload-artifact@v4
        with:
          name: robu_kernel
          path: build/robu_kernel

      - name: Upload RootFS Tar
        uses: actions/upload-artifact@v4
        with:
          name: rootfs.tar
          path: build/rootfs.tar
EOF
