/*
 * Cabeçalho Multiboot2 Specification (GRUB2 compatible)
 * Transiciona da Modo Protegido 32-bit enviado pelo GRUB para Long Mode 64-bit.
 */

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
#define MULTIBOOT2_ARCH_I386    0

.section .multiboot2, "a"
.align 8
mb2_header_start:
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT2_ARCH_I386
    .long mb2_header_end - mb2_header_start
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCH_I386 + (mb2_header_end - mb2_header_start))

    /* Tag de fim de cabeçalho */
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .text
.code32
.global mb2_start
.type mb2_start, @function
mb2_start:
    cli

    /* Salva informações do Multiboot2 passadas pelo GRUB */
    movl %eax, %eax     /* Magic (0x36d76289) */
    movl %ebx, %ebx     /* Ponteiro da Estrutura de Tags MB2 */

    /* Reutiliza o ponto de inicialização e montagem de paginação de arch/x86_64/src/boot.S */
    .extern start
    jmp start

.section .note.GNU-stack, "", @progbits
