#include "serial.h"
#include "ports.h"

#define PORT 0x3F8

void serial_init(void) {
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03);
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);
    outb(PORT + 2, 0xC7);
    outb(PORT + 4, 0x0B);
}

void serial_writechar(char c) {
    while (!(inb(PORT + 5) & 0x20));
    outb(PORT, c);
}

char serial_readchar(void) {
    while (!(inb(PORT + 5) & 1));
    return inb(PORT);
}

int serial_has_data(void) {
    return inb(PORT + 5) & 1;
}
