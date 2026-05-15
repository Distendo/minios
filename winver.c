#include "winver.h"
#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"

#define C_TEXT  RGB(0, 0, 0)
#define C_W     RGB(255, 255, 255)
#define C_G     RGB(0, 180, 0)
#define C_B     RGB(0, 60, 180)

static void draw_logo(int x, int y) {
    // Draw a simple "minios" logo: a blue "M" shape made of rectangles
    int s = 4; // scale
    fb_fill_rect(x, y, 7 * s, 9 * s, C_G);
    // cut out M shape from green background
    fb_fill_rect(x + s, y + s, s, 7 * s, C_W);
    fb_fill_rect(x + 3 * s, y + s, s, 7 * s, C_W);
    fb_fill_rect(x + 5 * s, y + s, s, 7 * s, C_W);
    // horizontal cuts
    fb_fill_rect(x + s, y + 3 * s, 5 * s, s, C_W);
    // center V of M
    fb_fill_rect(x + 2 * s, y + 4 * s, s, s, C_G);
    fb_fill_rect(x + 4 * s, y + 4 * s, s, s, C_G);
}

static void winver_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    fb_fill_rect(bx + 1, by + 1, bw - 2, bh - 2, C_W);

    draw_logo(bx + 12, by + 12);

    int x = bx + 12 + 32;
    int y = by + 14;

    fb_draw_string(x, y, "minios", RGB(0, 120, 0), C_W); y += 16;
    fb_draw_string(x, y, "Version 0.1 (Build 2026)", C_B, C_W); y += 14;
    fb_draw_string(x, y, "A minimal UNIX-like OS", C_TEXT, C_W); y += 14;
    fb_draw_string(x, y, "for x86 architecture", C_TEXT, C_W); y += 20;

    fb_draw_string(bx + 12, y, "Powered by:", C_TEXT, C_W); y += 14;
    fb_draw_string(bx + 12, y, "  GCC cross-compiler", C_TEXT, C_W); y += 14;
    fb_draw_string(bx + 12, y, "  NASM assembler", C_TEXT, C_W); y += 14;
    fb_draw_string(bx + 12, y, "  QEMU emulator", C_TEXT, C_W); y += 20;

    fb_draw_string(bx + 12, y, "Press ESC to close", RGB(128, 128, 128), C_W);
}

void winver_run(void) {
    int ww = 360, wh = 240;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    int win = gui_create_window(wx, wy, ww, wh, "About minios");
    if (win < 0) return;
    gui_mark_app_window(win);
    gui_window_set_redraw_fn(win, winver_redraw_fn);
    gui_redraw();

    int running = 1;
    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();
        if (c == 27) running = 0;
    }

    gui_redraw();
}