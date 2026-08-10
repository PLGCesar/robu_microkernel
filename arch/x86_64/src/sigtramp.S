.section .text
.global sigtramp_start
.global sigtramp_end
sigtramp_start:
    mov $8, %r8
    xor %edi, %edi
    xor %esi, %esi
    mov $0x800000, %edx
    int $0x30
1:
    jmp 1b
sigtramp_end:
