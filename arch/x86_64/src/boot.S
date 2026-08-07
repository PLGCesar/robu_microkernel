/*
 * Robu x86_64 boot path.
 *
 * QEMU's -kernel loader boots an ELF64 with a PVH note by entering `start`
 * in 32-bit protected mode: flat segments, paging OFF, interrupts off,
 * %ebx = PVH start_info (ignored for now). Everything the old stage-1A/1B
 * stub "assumed had already happened" is done here for real:
 *
 *   1. identity-map the first 1 GiB with 2 MiB pages (PML4 -> PDPT -> PD)
 *   2. CR4.PAE, EFER.LME, CR0.PG
 *   3. load a 64-bit GDT, far-jump into long mode
 *   4. set up the per-CPU kernel stack and call kmain
 *
 * The 16 KiB stack below is not just the boot stack: it is THE kernel
 * stack. Robu runs single-stack-per-CPU — every trap resets to its top
 * (see trap.S), and no kernel code path ever sleeps on it.
 */

/* PVH entry note (XEN_ELFNOTE_PHYS32_ENTRY = 18) */
.section .note.Xen, "a", @note
.align 4
    .long 4                 /* namesz: "Xen" + NUL */
    .long 4                 /* descsz: one 32-bit entry address */
    .long 18                /* XEN_ELFNOTE_PHYS32_ENTRY */
    .asciz "Xen"
    .long start

/* Page-table flag bits */
#define PTE_P   (1 << 0)
#define PTE_W   (1 << 1)
#define PTE_PS  (1 << 7)

.section .bss

/* The BSP's kernel stack (see file header). */
.align 16
.global kstack_bottom
.global kstack_top
kstack_bottom:
    .skip 16384
kstack_top:

/* The AP's own, entirely separate kernel stack -- stage 2C. Sharing
 * the BSP's would mean both cores pushing into the same 16 KiB region
 * the instant either one took a trap; per-CPU state starts here. */
.align 16
.global ap_kstack_bottom
.global ap_kstack_top
ap_kstack_bottom:
    .skip 16384
ap_kstack_top:

/* Early identity-map page tables, zeroed by the ELF loader. Global: the
 * VM layer copies this PML4's entry 0 into every address space it
 * creates so the kernel's identity map stays reachable after CR3
 * switches to a user process. */
.align 4096
.global boot_pml4
boot_pml4:
    .skip 4096
.global boot_pdpt
boot_pdpt:
    .skip 4096
boot_pd:
    .skip 4096

/* PVH hands us a struct hvm_start_info* in %ebx at entry (see start:).
 * Stashed here so arch_detect_memory() can read the real memory map
 * once we're far enough along to call C. */
.align 8
.global pvh_start_info_ptr
pvh_start_info_ptr:
    .skip 8

.section .rodata
.align 8
gdt64:
    .quad 0                         /* null */
    .quad 0x00209A0000000000        /* 0x08: kernel code, long mode */
    .quad 0x0000920000000000        /* 0x10: kernel data */
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64                     /* 32-bit base is fine: we link at 1 MiB */

.section .text
.code32
.global start
.type start, @function
start:
    cli
    /* %ebx = struct hvm_start_info* (PVH ABI). Save before it's clobbered;
       upper 32 bits of the .bss qword are already zero. */
    movl %ebx, pvh_start_info_ptr

    movl $kstack_top, %esp

    /* PML4[0] -> PDPT, PDPT[0] -> PD (upper words stay zero from .bss) */
    movl $boot_pdpt + (PTE_P | PTE_W), boot_pml4
    movl $boot_pd + (PTE_P | PTE_W), boot_pdpt

    /* PD[0..511]: 512 x 2 MiB identity pages = first 1 GiB */
    movl $boot_pd, %edi
    movl $(PTE_P | PTE_W | PTE_PS), %eax
    movl $512, %ecx
1:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 1b

    /* Point CR3 at the new tables */
    movl $boot_pml4, %eax
    movl %eax, %cr3

    /* CR4.PAE */
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4

    /* EFER.LME | EFER.NXE. NXE (bit 11) has to be set here, at boot,
       because it's what makes the NX bit (PTE bit 63) mean anything --
       without it, bit 63 is a *reserved* bit, and vm_arch.c has been
       setting it on every non-executable mapping since stage 1B. A
       PTE with a reserved bit set takes a #PF the instant it's used,
       even though it looks perfectly present and correctly formed. */
    movl $0xC0000080, %ecx
    rdmsr
    orl $((1 << 8) | (1 << 11)), %eax
    wrmsr

    /* CR0.PG (PE is already set) -> we are now in compatibility mode */
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    /* Load our GDT and jump into a 64-bit code segment */
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

    /* kmain never returns; if it does, park the CPU. */
    cli
1:  hlt
    jmp 1b

.section .note.GNU-stack, "", @progbits
