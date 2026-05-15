#include "keyboard.h"
#include "ports.h"

#define BUF_SIZE 256

static unsigned char scancode;
static char buf[BUF_SIZE];
static int head, tail;
static int left_shift, right_shift, caps;

static const char sc_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const char sc_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

void keyboard_init(void) {
    head = 0;
    tail = 0;
    left_shift = 0;
    right_shift = 0;
    caps = 0;
}

static void keyboard_push(char c) {
    int next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        buf[head] = c;
        head = next;
    }
}

void keyboard_poll(void) {
    uint8_t s = inb(0x64);
    if (!(s & 1) || (s & 0x20)) return;

    scancode = inb(0x60);

    if (scancode == 0x2A) left_shift = 1;
    if (scancode == 0x36) right_shift = 1;
    if (scancode == 0xAA) left_shift = 0;
    if (scancode == 0xB6) right_shift = 0;
    if (scancode == 0x3A) caps = !caps;

    if (scancode < 0x80) {
        char c = 0;
        if ((left_shift || right_shift) || caps) {
            if (scancode < sizeof(sc_shift))
                c = sc_shift[scancode];
        } else {
            if (scancode < sizeof(sc_ascii))
                c = sc_ascii[scancode];
        }
        if (c) keyboard_push(c);
        if (scancode == 0x0E) keyboard_push('\b');
    }
}

char keyboard_getchar(void) {
    if (head == tail) return 0;
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
