#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t *addr;
    int width;
    int height;
    int pitch;
    int bpp;
} fb_info_t;

extern fb_info_t fb;

int fb_init(void);
void fb_putpixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);
void fb_clear(uint32_t color);

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#endif
