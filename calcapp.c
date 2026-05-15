#include "calcapp.h"
#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"

#define C_BG    RGB(240, 240, 240)
#define C_BTN   RGB(220, 220, 220)
#define C_BTN2  RGB(200, 200, 200)
#define C_OP    RGB(180, 200, 230)
#define C_EQ    RGB(50, 120, 200)
#define C_CLR   RGB(200, 100, 80)
#define C_TEXT  RGB(0, 0, 0)
#define C_DISP  RGB(255, 255, 255)

#define BTN_W 48
#define BTN_H 32
#define BTN_GAP 4
#define COLS 4
#define ROWS 5

static const char *btn_labels[ROWS][COLS] = {
    {"7", "8", "9", "/"},
    {"4", "5", "6", "*"},
    {"1", "2", "3", "-"},
    {"0", ".", "C", "+"},
    {"",  "",  "",  "="},
};

static void calcapp_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    fb_fill_rect(bx + 1, by + 1, bw - 2, bh - 2, C_BG);

    int disp_h = 36;
    int pad_x = (bw - (COLS * BTN_W + (COLS - 1) * BTN_GAP)) / 2;
    int base_y = by + disp_h + 14;

    int disp_x = bx + 6;
    int disp_y = by + 6;
    fb_fill_rect(disp_x, disp_y, bw - 12, disp_h, RGB(0,0,0));

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            const char *lbl = btn_labels[r][c];
            if (!lbl || !*lbl) continue;
            int x = bx + pad_x + c * (BTN_W + BTN_GAP);
            int y = base_y + r * (BTN_H + BTN_GAP);
            uint32_t col;
            if (lbl[0] == '=') col = C_EQ;
            else if (lbl[0] == 'C') col = C_CLR;
            else if (lbl[0] == '+' || lbl[0] == '-' || lbl[0] == '*' || lbl[0] == '/') col = C_OP;
            else col = C_BTN;
            fb_fill_rect(x, y, BTN_W, BTN_H, col);
            int tw = 0; while (lbl[tw]) tw++;
            fb_draw_string(x + (BTN_W - tw * 8) / 2, y + (BTN_H - 8) / 2, lbl, C_TEXT, col);
        }
    }
}

static double parse_display(const char *s) {
    double val = 0;
    int neg = 0, dec = 0;
    double div = 1;
    if (*s == '-') { neg = 1; s++; }
    while (*s) {
        if (*s == '.') { dec = 1; s++; continue; }
        if (dec) { div *= 10; val += (double)(*s - '0') / div; }
        else val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

static void format_display(char *buf, double val) {
    int di = 0;
    if (val < 0) { buf[di++] = '-'; val = -val; }
    int whole = (int)val;
    if (whole == 0) buf[di++] = '0';
    else {
        char rev[16]; int ri = 0;
        while (whole > 0) { rev[ri++] = '0' + (whole % 10); whole /= 10; }
        while (ri > 0) buf[di++] = rev[--ri];
    }
    double frac = val - (int)val;
    if (frac > 0.0001) {
        buf[di++] = '.';
        for (int i = 0; i < 4 && frac > 0.0001; i++) {
            frac *= 10;
            int d = (int)frac;
            buf[di++] = '0' + d;
            frac -= d;
        }
    }
    buf[di] = 0;
}

static void draw_display(int win, const char *display) {
    int bx, by, bw, bh;
    gui_get_body(win, &bx, &by, &bw, &bh);
    int disp_w = bw - 12;
    int disp_h = 36;
    int disp_x = bx + 6;
    int disp_y = by + 6;
    fb_fill_rect(disp_x, disp_y, disp_w, disp_h, RGB(0,0,0));
    int len = 0; while (display[len]) len++;
    fb_draw_string(disp_x + disp_w - len * 8 - 4, disp_y + (disp_h - 8) / 2, display, RGB(0, 255, 0), RGB(0,0,0));
}

void calcapp_run(void) {
    int ww = COLS * BTN_W + (COLS - 1) * BTN_GAP + 20;
    int wh = 36 + 14 + ROWS * (BTN_H + BTN_GAP) + 30;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    int win = gui_create_window(wx, wy, ww, wh, "Calculator");
    if (win < 0) return;
    gui_mark_app_window(win);
    gui_window_set_redraw_fn(win, calcapp_redraw_fn);

    char display[32] = "0";
    double accum = 0;
    char last_op = 0;
    int fresh = 1;
    int has_decimal = 0;

    gui_redraw();

    int running = 1;
    int handled;
    while (running) {
        gui_poll();
        keyboard_poll();
        mouse_poll();

        int mx, my;
        mouse_get_pos(&mx, &my);
        int btns = mouse_get_buttons();

        static int prev_btns = 0;
        handled = 0;

        if ((btns & 1) && !(prev_btns & 1)) {
            int bx, by, bw, bh;
            gui_get_body(win, &bx, &by, &bw, &bh);
            int pad_x = (bw - (COLS * BTN_W + (COLS - 1) * BTN_GAP)) / 2;
            int base_y = by + 36 + 14;

            for (int r = 0; r < ROWS; r++) {
                for (int c = 0; c < COLS; c++) {
                    const char *lbl = btn_labels[r][c];
                    if (!lbl || !*lbl) continue;
                    int x = bx + pad_x + c * (BTN_W + BTN_GAP);
                    int y = base_y + r * (BTN_H + BTN_GAP);
                    if (mx >= x && mx < x + BTN_W && my >= y && my < y + BTN_H) {
                        char ch = lbl[0];
                        if (ch >= '0' && ch <= '9') {
                            if (fresh) { display[0] = ch; display[1] = 0; fresh = 0; }
                            else { int len = 0; while (display[len]) len++; if (len < 15) { display[len] = ch; display[len+1] = 0; } }
                        } else if (ch == '.') {
                            if (!has_decimal) {
                                if (fresh) { display[0] = '0'; display[1] = '.'; display[2] = 0; fresh = 0; }
                                else { int len = 0; while (display[len]) len++; if (len < 15) { display[len] = '.'; display[len+1] = 0; } }
                                has_decimal = 1;
                            }
                        } else if (ch == 'C') {
                            display[0] = '0'; display[1] = 0;
                            accum = 0; last_op = 0; fresh = 1; has_decimal = 0;
                        } else if (ch == '=') {
                            double cur = parse_display(display);
                            if (last_op == '+') accum += cur;
                            else if (last_op == '-') accum -= cur;
                            else if (last_op == '*') accum *= cur;
                            else if (last_op == '/') { if (cur != 0) accum /= cur; }
                            else accum = cur;
                            last_op = 0;
                            format_display(display, accum);
                            fresh = 1; has_decimal = 0;
                        } else {
                            double cur = parse_display(display);
                            if (last_op == '+') accum += cur;
                            else if (last_op == '-') accum -= cur;
                            else if (last_op == '*') accum *= cur;
                            else if (last_op == '/') { if (cur != 0) accum /= cur; }
                            else accum = cur;
                            last_op = ch;
                            format_display(display, accum);
                            fresh = 1; has_decimal = 0;
                        }
                        draw_display(win, display);
                        handled = 1;
                        break;
                    }
                }
                if (handled) break;
            }
        }
        prev_btns = btns;

        if (!handled) {
            char c = keyboard_getchar();
            if (c == 27) running = 0;
            else if (c >= '0' && c <= '9') {
                if (fresh) { display[0] = c; display[1] = 0; fresh = 0; }
                else { int len = 0; while (display[len]) len++; if (len < 15) { display[len] = c; display[len+1] = 0; } }
                draw_display(win, display);
            } else if (c == '.') {
                if (!has_decimal) {
                    if (fresh) { display[0] = '0'; display[1] = '.'; display[2] = 0; fresh = 0; }
                    else { int len = 0; while (display[len]) len++; if (len < 15) { display[len] = '.'; display[len+1] = 0; } }
                    has_decimal = 1;
                    draw_display(win, display);
                }
            } else if (c == 'c' || c == 'C') {
                display[0] = '0'; display[1] = 0;
                accum = 0; last_op = 0; fresh = 1; has_decimal = 0;
                draw_display(win, display);
            } else if (c == '\n' || c == '=') {
                double cur = parse_display(display);
                if (last_op == '+') accum += cur;
                else if (last_op == '-') accum -= cur;
                else if (last_op == '*') accum *= cur;
                else if (last_op == '/') { if (cur != 0) accum /= cur; }
                else accum = cur;
                last_op = 0;
                format_display(display, accum);
                fresh = 1; has_decimal = 0;
                draw_display(win, display);
            } else if (c == '+' || c == '-' || c == '*' || c == '/') {
                double cur = parse_display(display);
                if (last_op == '+') accum += cur;
                else if (last_op == '-') accum -= cur;
                else if (last_op == '*') accum *= cur;
                else if (last_op == '/') { if (cur != 0) accum /= cur; }
                else accum = cur;
                last_op = c;
                format_display(display, accum);
                fresh = 1; has_decimal = 0;
                draw_display(win, display);
            } else if (c == '\b') {
                int len = 0; while (display[len]) len++;
                if (len > 1) {
                    if (display[len-1] == '.') has_decimal = 0;
                    display[len-1] = 0;
                } else {
                    display[0] = '0'; display[1] = 0;
                    fresh = 1;
                }
                draw_display(win, display);
            }
        }
    }

    gui_redraw();
}
