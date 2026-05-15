#include "framebuffer.h"
#include "ports.h"
#include "font.h"

fb_info_t fb;

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

static void vbe_write(int index, uint16_t val) {
    outw(0x1CE, index);
    outw(0x1CF, val);
}

static uint16_t vbe_read(int index) {
    outw(0x1CE, index);
    return inw(0x1CF);
}

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t addr = 0x80000000 | (bus << 16) | ((dev & 31) << 11) | ((func & 7) << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

int fb_init(void) {
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id != 0xB0C0 && id != 0xB0C1 && id != 0xB0C2 && id != 0xB0C3
        && id != 0xB0C4 && id != 0xB0C5) {
        return 0;
    }

    vbe_write(VBE_DISPI_INDEX_ENABLE, 0);
    vbe_write(VBE_DISPI_INDEX_XRES, 800);
    vbe_write(VBE_DISPI_INDEX_YRES, 600);
    vbe_write(VBE_DISPI_INDEX_BPP, 32);
    vbe_write(VBE_DISPI_INDEX_ENABLE, 0x41);

    fb.width = 800;
    fb.height = 600;
    fb.bpp = 32;
    fb.pitch = 800 * 4;

    uint32_t lfb = 0;
    for (int dev = 0; dev < 32; dev++) {
        uint32_t idw = pci_read(0, dev, 0, 0);
        if ((idw & 0xFFFF) == 0xFFFF) continue;
        uint32_t class = pci_read(0, dev, 0, 8);
        if (((class >> 24) & 0xFF) == 0x03 && ((class >> 16) & 0xFF) == 0x00) {
            uint32_t bar0 = pci_read(0, dev, 0, 0x10);
        if (!(bar0 & 1)) {
            lfb = bar0 & 0xFFFFFFF0;
            break;
        }
        }
    }

    if (!lfb) lfb = 0xE0000000;
    fb.addr = (uint32_t*)lfb;
    return 1;
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= fb.width || y < 0 || y >= fb.height) return;
    fb.addr[y * (fb.pitch / 4) + x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb.width) w = fb.width - x;
    if (y + h > fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            fb.addr[(y + row) * (fb.pitch / 4) + (x + col)] = color;
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c < ' ' || c > '~') c = ' ';
    int idx = (c - ' ') * 8;
    for (int row = 0; row < 8; row++) {
        unsigned char bits = font8x8[idx + row];
        for (int col = 0; col < 8; col++) {
            fb_putpixel(x + col, y + row, (bits & (1 << (7 - col))) ? fg : bg);
        }
    }
}

void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str >= ' ' && *str <= '~')
            fb_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

void fb_clear(uint32_t color) {
    for (int i = 0; i < fb.width * fb.height; i++)
        fb.addr[i] = color;
}
