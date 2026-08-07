#ifndef ARCH_X86_64_PORTIO_H
#define ARCH_X86_64_PORTIO_H
#include "robu/types.h"
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void io_wait(void) {
    outb(0x80, 0);
}
#endif
