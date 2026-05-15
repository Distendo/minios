#include "vga.h"
#include "serial.h"
#include "gui.h"
#include "ports.h"
#include <stddef.h>

static uint16_t *vga_mem = (uint16_t*)0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
static int row, col;
static uint8_t color;

extern int gui_mode;

void vga_init(void) {
    row = 0;
    col = 0;
    color = vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_setcolor(uint8_t c) {
    color = c;
}

static void vga_putentryat(char ch, uint8_t col, int r, int c) {
    vga_mem[r * VGA_WIDTH + c] = vga_entry(ch, col);
}

static void vga_scroll(void) {
    for (int r = 0; r < VGA_HEIGHT - 1; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            vga_mem[r * VGA_WIDTH + c] = vga_mem[(r + 1) * VGA_WIDTH + c];
    for (int c = 0; c < VGA_WIDTH; c++)
        vga_putentryat(' ', color, VGA_HEIGHT - 1, c);
    if (row > 0) row--;
}

static void vga_update_cursor(void) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_putchar(char c) {
    serial_writechar(c);

    if (gui_mode) {
        gui_term_putchar(c);
        return;
    }

    if (c == '\n') {
        col = 0;
        row++;
        if (row >= VGA_HEIGHT) vga_scroll();
        vga_update_cursor();
        return;
    }
    if (c == '\b') {
        if (col > 0) col--;
        vga_putentryat(' ', color, row, col);
        vga_update_cursor();
        return;
    }
    if (c < ' ') return;

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
        if (row >= VGA_HEIGHT) vga_scroll();
    }
    vga_putentryat(c, color, row, col);
    col++;
    vga_update_cursor();
}

void vga_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        vga_putchar(data[i]);
}

void vga_writestring(const char *data) {
    size_t len = 0;
    while (data[len]) len++;
    vga_write(data, len);
}

void vga_clear(void) {
    for (int r = 0; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            vga_putentryat(' ', color, r, c);
    row = 0;
    col = 0;
    vga_update_cursor();
}
