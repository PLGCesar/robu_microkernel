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
