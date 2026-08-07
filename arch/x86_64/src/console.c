#include "robu/types.h"
#include "robu/kprintf.h"
#include "robu/spinlock.h"
#include "portio.h"
#define COM1 0x3F8
static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {
    }
    outb(COM1, (uint8_t)c);
}
static int serial_getc(void) {
    if (!(inb(COM1 + 5) & 0x01)) {
        return -1;
    }
    return (int)(uint8_t)inb(COM1);
}
#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_ATTR 0x07
static volatile uint16_t *const vga_mem = (volatile uint16_t *)0xB8000;
static int vga_row;
static int vga_col;
static void vga_scroll(void) {
    for (int r = 1; r < VGA_ROWS; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            vga_mem[(r - 1) * VGA_COLS + c] = vga_mem[r * VGA_COLS + c];
        }
    }
    for (int c = 0; c < VGA_COLS; c++) {
        vga_mem[(VGA_ROWS - 1) * VGA_COLS + c] = (VGA_ATTR << 8) | ' ';
    }
    vga_row = VGA_ROWS - 1;
}
static void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row >= VGA_ROWS) {
            vga_scroll();
        }
        return;
    }
    vga_mem[vga_row * VGA_COLS + vga_col] = (uint16_t)((VGA_ATTR << 8) | (uint8_t)c);
    if (++vga_col >= VGA_COLS) {
        vga_col = 0;
        if (++vga_row >= VGA_ROWS) {
            vga_scroll();
        }
    }
}
static void vga_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        vga_mem[i] = (VGA_ATTR << 8) | ' ';
    }
    vga_row = 0;
    vga_col = 0;
}
void arch_console_init(void) {
    serial_init();
    vga_clear();
}
void arch_console_putc(char c) {
    if (c == '\n') {
        serial_putc('\r');
    }
    serial_putc(c);
    vga_putc(c);
}
int arch_console_getc(void) {
    return serial_getc();
}
static spinlock_t console_ring_lock = SPINLOCK_INIT;
#define CONSOLE_LINE_MAX 128
#define CONSOLE_RING_SIZE 256
static char console_ring[CONSOLE_RING_SIZE];
static uint32_t ring_head, ring_tail;
static char line_buf[CONSOLE_LINE_MAX];
static int line_len;
static void ring_push_locked(char c) {
    uint32_t next = (ring_head + 1) % CONSOLE_RING_SIZE;
    if (next == ring_tail) {
        return;
    }
    console_ring[ring_head] = c;
    ring_head = next;
}
void arch_console_line_feed(int c) {
    if (c == '\b' || c == 0x7F) {
        if (line_len > 0) {
            line_len--;
            arch_console_putc('\b');
            arch_console_putc(' ');
            arch_console_putc('\b');
        }
        return;
    }
    if (c == '\r' || c == '\n') {
        arch_console_putc('\n');
        spin_lock(&console_ring_lock);
        for (int i = 0; i < line_len; i++) {
            ring_push_locked(line_buf[i]);
        }
        ring_push_locked('\n');
        spin_unlock(&console_ring_lock);
        line_len = 0;
        return;
    }
    if (line_len < CONSOLE_LINE_MAX - 1) {
        line_buf[line_len++] = (char)c;
        arch_console_putc((char)c);
    }
}
int arch_console_read_line_bytes(uint8_t *out, int max) {
    spin_lock(&console_ring_lock);
    int n = 0;
    while (n < max && ring_tail != ring_head) {
        out[n++] = (uint8_t)console_ring[ring_tail];
        ring_tail = (ring_tail + 1) % CONSOLE_RING_SIZE;
    }
    spin_unlock(&console_ring_lock);
    return n;
}
