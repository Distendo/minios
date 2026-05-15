#include "mouse.h"
#include "ports.h"

#define STATUS 0x64
#define DATA   0x60

static int mouse_x, mouse_y;
static int mouse_buttons;
static int packet_state;
static int packet[3];
static int mouse_initialized;

void mouse_init(void) {
    while (inb(STATUS) & 2);

    outb(STATUS, 0xA8);
    io_wait();
    while (inb(STATUS) & 2);

    outb(STATUS, 0x20);
    while (!(inb(STATUS) & 1));
    uint8_t config = inb(DATA);
    config |= 0x02;
    config &= ~0x20;

    while (inb(STATUS) & 2);
    outb(STATUS, 0x60);
    while (inb(STATUS) & 2);
    outb(DATA, config);

    while (inb(STATUS) & 2);
    outb(STATUS, 0xD4);
    io_wait();
    while (inb(STATUS) & 2);
    outb(DATA, 0xF4);
    while (!(inb(STATUS) & 1));
    inb(DATA);

    mouse_x = 400;
    mouse_y = 300;
    mouse_buttons = 0;
    packet_state = 0;
    mouse_initialized = 1;
}

void mouse_handle_byte(uint8_t data) {
    if (!mouse_initialized) return;

    packet[packet_state++] = data;

    if (packet_state == 3) {
        int dx = packet[1];
        int dy = packet[2];
        if (packet[0] & 0x10) dx |= 0xFFFFFF00;
        if (packet[0] & 0x20) dy |= 0xFFFFFF00;
        dy = -dy;

        mouse_x += dx;
        mouse_y += dy;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= 800) mouse_x = 799;
        if (mouse_y >= 600) mouse_y = 599;

        mouse_buttons = packet[0] & 0x07;
        packet_state = 0;
    }
}

void mouse_poll(void) {
    uint8_t s = inb(STATUS);
    if ((s & 1) && (s & 0x20))
        mouse_handle_byte(inb(DATA));
}

void mouse_get_pos(int *x, int *y) {
    *x = mouse_x;
    *y = mouse_y;
}

int mouse_get_buttons(void) {
    return mouse_buttons;
}
