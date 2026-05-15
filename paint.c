#include "paint.h"
#include "gui.h"
#include "framebuffer.h"
#include "mouse.h"
#include "keyboard.h"
#include "ports.h"

#define COLORS 9
static uint32_t palette[COLORS] = {
    RGB(0, 0, 0), RGB(180, 0, 0), RGB(0, 180, 0), RGB(0, 0, 180),
    RGB(255, 255, 0), RGB(255, 0, 255), RGB(0, 255, 255),
    RGB(255, 255, 255), RGB(128, 128, 128)
};

static int sel_color;
static int prev_mx, prev_my, prev_btn;
static int paint_win;
static int canvas_bx, canvas_by, canvas_bw, canvas_bh;

static void draw_palette(int bx, int by) {
    for (int i = 0; i < COLORS; i++) {
        int px = bx + i * 28;
        int py = by;
        uint32_t border = (i == sel_color) ? RGB(255, 255, 255) : RGB(80, 80, 80);
        fb_fill_rect(px, py, 26, 26, palette[i]);
        for (int d = 0; d < 26; d++) {
            fb_putpixel(px + d, py, border);
            fb_putpixel(px + d, py + 25, border);
            fb_putpixel(px, py + d, border);
            fb_putpixel(px + 25, py + d, border);
        }
    }
}

static void paint_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    int cy = by + 32;
    int ch = bh - 32;
    fb_fill_rect(bx + 1, cy, bw - 2, ch, RGB(255, 255, 255));
    draw_palette(bx + 4, by + 4);
}

void paint_run(void) {
    int wx = 40, wy = 40;
    int ww = 640, wh = 480;
    paint_win = gui_create_window(wx, wy, ww, wh, "Paint");
    if (paint_win < 0) return;
    gui_mark_app_window(paint_win);
    gui_window_set_redraw_fn(paint_win, paint_redraw_fn);
    sel_color = 7;

    int bx, by, bw, bh;
    gui_get_body(paint_win, &bx, &by, &bw, &bh);

    canvas_bx = bx + 1;
    canvas_by = by + 32;
    canvas_bw = bw - 2;
    canvas_bh = bh - 32;
    fb_fill_rect(canvas_bx, canvas_by, canvas_bw, canvas_bh, RGB(255, 255, 255));
    draw_palette(bx + 4, by + 4);
    gui_redraw();

    prev_mx = prev_my = -1;
    prev_btn = 0;
    int running = 1;
    int drawing = 0;

    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();
        if (c == 27) running = 0;
        if (c == 'c' || c == 'C') {
            fb_fill_rect(canvas_bx, canvas_by, canvas_bw, canvas_bh, RGB(255, 255, 255));
        }

        int mx, my;
        mouse_get_pos(&mx, &my);
        int btns = mouse_get_buttons();

        if ((btns & 1) && !(prev_btn & 1)) {
            if (my >= by + 4 && my < by + 30 && mx >= bx + 4 && mx < bx + 4 + COLORS * 28) {
                int ci = (mx - bx - 4) / 28;
                if (ci >= 0 && ci < COLORS) {
                    sel_color = ci;
                    draw_palette(bx + 4, by + 4);
                }
            } else if (mx > canvas_bx && mx < canvas_bx + canvas_bw && my > canvas_by && my < canvas_by + canvas_bh) {
                drawing = 1;
            }
        }

        if (!(btns & 1)) drawing = 0;

        if (drawing && mx > canvas_bx && mx < canvas_bx + canvas_bw && my > canvas_by && my < canvas_by + canvas_bh) {
            gui_cursor_hide();
            fb_putpixel(mx, my, palette[sel_color]);
            if (prev_mx >= 0) {
                int x0 = prev_mx, y0 = prev_my, x1 = mx, y1 = my;
                int dx = x1 > x0 ? x1 - x0 : x0 - x1;
                int dy = y1 > y0 ? y1 - y0 : y0 - y1;
                int steps = dx > dy ? dx : dy;
                if (steps < 1) steps = 1;
                for (int i = 1; i <= steps; i++) {
                    int xi = x0 + (x1 - x0) * i / steps;
                    int yi = y0 + (y1 - y0) * i / steps;
                    fb_putpixel(xi, yi, palette[sel_color]);
                }
            }
            prev_mx = mx;
            prev_my = my;
            gui_cursor_show();
        } else {
            prev_mx = prev_my = -1;
        }

        prev_btn = btns;
    }

    gui_redraw();
}