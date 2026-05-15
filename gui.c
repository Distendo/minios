#include "gui.h"
#include "fs.h"
#include "framebuffer.h"
#include "mouse.h"
#include "keyboard.h"
#include "ports.h"
#include "paint.h"
#include "snake.h"
#include "browser.h"
#include "settings.h"
#include "rtc.h"
#include "files.h"
#include "winver.h"
#include "calcapp.h"
#include "notepad.h"
#include "settings_store.h"

#define TITLE_H 18
#define TASKBAR 22
#define BORDER  2
#define MAX_WINS 12
#define RESIZE_H 10

#define COL_DESKTOP    RGB(51, 102, 153)
#define COL_TASKBAR    RGB(40, 40, 40)
#define COL_TITLE_ACT  RGB(0, 60, 180)
#define COL_TITLE_IN   RGB(80, 80, 80)
#define COL_WIN_BG     RGB(240, 240, 240)
#define COL_TEXT       RGB(0, 0, 0)
#define COL_WHITE      RGB(255, 255, 255)
#define COL_BLACK      RGB(0, 0, 0)
#define COL_RED        RGB(180, 0, 0)
#define COL_TITLE_TEXT RGB(255, 255, 255)

#define TERM_ROWS 100
#define TERM_COLS 80

typedef struct {
    int x, y, w, h;
    char title[32];
    int visible;
    int minimized;
    char buf[TERM_ROWS][TERM_COLS];
    int buf_row, buf_col;
    int is_app;
    window_draw_fn redraw_fn;
} window_t;

static window_t wins[MAX_WINS];
static int num_wins;
static int active_win;
static int drag_win;
static int drag_ox, drag_oy;

static int resize_win;
static int resize_ox, resize_oy;
static int resize_ow, resize_oh;

static int gui_active;

static int cursor_x = -1, cursor_y = -1;
static int cursor_visible;

// Drag & Drop state
static char drag_file[FS_NAME_LEN];
static int drag_active;
static int drag_from_win;

#define APP_BTN_W 55
#define APP_MENU_W 120
#define APP_MENU_ITEM_H 18
#define APP_COUNT 9
static const char *app_items[APP_COUNT] = {"Terminal", "Paint", "Snake", "Browser", "Settings", "Files", "Winver", "Calc", "Notepad"};
static int apps_menu_open;

static void term_clear(window_t *w) {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            w->buf[r][c] = ' ';
    w->buf_row = 0;
    w->buf_col = 0;
}

static void term_scroll(window_t *w) {
    for (int r = 0; r < TERM_ROWS - 1; r++)
        for (int c = 0; c < TERM_COLS; c++)
            w->buf[r][c] = w->buf[r + 1][c];
    for (int c = 0; c < TERM_COLS; c++)
        w->buf[TERM_ROWS - 1][c] = ' ';
    if (w->buf_row > 0) w->buf_row--;
}

static void term_putchar(window_t *w, char c) {
    if (c == '\n') {
        w->buf_col = 0;
        w->buf_row++;
        if (w->buf_row >= TERM_ROWS) term_scroll(w);
        return;
    }
    if (c == '\b') {
        if (w->buf_col > 0) w->buf_col--;
        w->buf[w->buf_row][w->buf_col] = ' ';
        return;
    }
    if (c < ' ') return;
    if (w->buf_col >= TERM_COLS) {
        w->buf_col = 0;
        w->buf_row++;
        if (w->buf_row >= TERM_ROWS) term_scroll(w);
    }
    w->buf[w->buf_row][w->buf_col++] = c;
}

static void term_writestring(window_t *w, const char *s) {
    while (*s) term_putchar(w, *s++);
}

// --- Cursor drawing ---

static void xor_pixel(int x, int y) {
    if (x >= 0 && x < fb.width && y >= 0 && y < fb.height)
        fb.addr[y * (fb.pitch / 4) + x] ^= 0x00FFFFFF;
}

static void cursor_draw_cross(int mx, int my) {
    for (int i = -4; i <= 4; i++) {
        xor_pixel(mx + i, my);
        xor_pixel(mx, my + i);
    }
    xor_pixel(mx, my);
}

static void cursor_draw_arrow(int mx, int my) {
    xor_pixel(mx, my);
    xor_pixel(mx + 1, my + 1);
    xor_pixel(mx + 2, my + 2);
    xor_pixel(mx + 3, my + 3);
    xor_pixel(mx + 4, my + 4);
    xor_pixel(mx - 1, my + 2);
    xor_pixel(mx - 1, my + 3);
    xor_pixel(mx, my + 4);
    xor_pixel(mx, my + 5);
    xor_pixel(mx + 1, my + 5);
}

static void cursor_draw_hand(int mx, int my) {
    for (int dy = 0; dy < 3; dy++)
        for (int dx = 0; dx < 3; dx++)
            xor_pixel(mx + dx, my + dy);
    xor_pixel(mx + 4, my);
    xor_pixel(mx + 5, my);
    xor_pixel(mx + 4, my + 1);
    xor_pixel(mx - 1, my + 1);
    xor_pixel(mx - 2, my + 2);
    xor_pixel(mx + 5, my + 2);
    xor_pixel(mx, my + 3);
    xor_pixel(mx + 1, my + 4);
    xor_pixel(mx + 2, my + 5);
}

static void cursor_xor_at(int mx, int my) {
    if (gui_settings.mouse_icon == 0) cursor_draw_cross(mx, my);
    else if (gui_settings.mouse_icon == 1) cursor_draw_arrow(mx, my);
    else cursor_draw_hand(mx, my);
}

static void cursor_update(void) {
    if (!cursor_visible) return;
    int mx, my;
    mouse_get_pos(&mx, &my);
    if (mx == cursor_x && my == cursor_y) return;
    if (cursor_x >= 0) cursor_xor_at(cursor_x, cursor_y);
    cursor_xor_at(mx, my);
    cursor_x = mx;
    cursor_y = my;
}

// --- Drawing helpers ---

static void draw_background(void) {
    fb_clear(gui_settings.bg_color);
    if (gui_settings.bg_pattern == 0) {
        uint32_t dot = RGB(60, 110, 160);
        for (int y = 0; y < fb.height - TASKBAR; y += 16)
            for (int x = 0; x < fb.width; x += 16)
                if ((x / 16 + y / 16) % 2 == 0)
                    fb_putpixel(x, y, dot);
    } else if (gui_settings.bg_pattern == 2) {
        uint32_t stripe = RGB(60, 110, 160);
        for (int y = 0; y < fb.height - TASKBAR; y += 12)
            fb_fill_rect(0, y, fb.width, 2, stripe);
    }
}

static void draw_titlebar(window_t *w) {
    uint32_t tc = (&wins[active_win] == w) ? COL_TITLE_ACT : COL_TITLE_IN;
    fb_fill_rect(w->x, w->y, w->w, TITLE_H, tc);
    int tx = w->x + 4;
    int ty = w->y + (TITLE_H - 8) / 2;
    fb_draw_string(tx, ty, w->title, COL_TITLE_TEXT, tc);

    int by = w->y + (TITLE_H - 14) / 2;

    int cx = w->x + w->w - 18;
    int cy = by;
    fb_fill_rect(cx, cy, 14, 14, COL_RED);
    fb_draw_string(cx + 3, cy + 3, "X", COL_WHITE, COL_RED);

    int mx = cx - 16;
    int mcy = cy;
    fb_fill_rect(mx, mcy, 14, 14, RGB(200, 180, 0));
    fb_fill_rect(mx + 2, mcy + 6, 10, 2, COL_WHITE);
}

static void draw_window(window_t *w) {
    if (!w->visible) return;

    int shadow = 2;
    fb_fill_rect(w->x + shadow, w->y + shadow, w->w, w->h, COL_BLACK);
    draw_titlebar(w);

    if (w->is_app) {
        if (w->redraw_fn)
            w->redraw_fn(&wins[0] - &wins[0] + (w - wins));
    } else {
        int body_x = w->x;
        int body_y = w->y + TITLE_H;
        int body_w = w->w;
        int body_h = w->h - TITLE_H;
        fb_fill_rect(body_x, body_y, body_w, body_h, COL_WIN_BG);

        int cols = (body_w - 4) / 8;
        if (cols > TERM_COLS) cols = TERM_COLS;
        int rows = body_h / 8;

        int start_row = w->buf_row - rows + 1;
        if (start_row < 0) start_row = 0;

        for (int r = 0; r < rows && start_row + r < TERM_ROWS; r++)
            for (int c = 0; c < cols; c++) {
                char ch = w->buf[start_row + r][c];
                if (ch == 0) ch = ' ';
                fb_draw_char(body_x + 2 + c * 8, body_y + 2 + r * 8,
                             ch, COL_TEXT, COL_WIN_BG);
            }

        int cursor_r = w->buf_row - start_row;
        int cursor_c = w->buf_col;
        if (cursor_r >= 0 && cursor_r < rows && cursor_c < cols) {
            int cx = body_x + 2 + cursor_c * 8;
            int cy = body_y + 2 + cursor_r * 8;
            fb_fill_rect(cx + 6, cy, 2, 8, COL_BLACK);
        }
    }
}

static void draw_apps_menu(void) {
    if (!apps_menu_open) return;
    int y = fb.height - TASKBAR;
    int mx = 5;
    int my = y - APP_COUNT * APP_MENU_ITEM_H - 4;
    int mw = APP_MENU_W;
    int mh = APP_COUNT * APP_MENU_ITEM_H + 4;
    fb_fill_rect(mx, my, mw, mh, RGB(50, 50, 50));
    fb_fill_rect(mx + 1, my + 1, mw - 2, mh - 2, RGB(240, 240, 240));
    for (int i = 0; i < APP_COUNT; i++) {
        int iy = my + 2 + i * APP_MENU_ITEM_H;
        fb_fill_rect(mx + 2, iy, mw - 4, APP_MENU_ITEM_H - 1, RGB(240, 240, 240));
        fb_draw_string(mx + 8, iy + 4, app_items[i], COL_TEXT, RGB(240, 240, 240));
    }
}

static void draw_taskbar(void) {
    int y = fb.height - TASKBAR;
    fb_fill_rect(0, y, fb.width, TASKBAR, COL_TASKBAR);

    uint32_t app_col = apps_menu_open ? RGB(0, 80, 180) : RGB(60, 60, 60);
    fb_fill_rect(4, y + 2, APP_BTN_W, TASKBAR - 4, app_col);
    fb_draw_string(10, y + (TASKBAR - 8) / 2, "Apps", COL_WHITE, app_col);

    int bx = 4 + APP_BTN_W + 6;
    for (int i = 0; i < num_wins; i++) {
        if (!wins[i].visible && !wins[i].minimized) continue;
        int bw = 100;
        uint32_t bg = (i == active_win) ? RGB(60, 60, 60) : RGB(80, 80, 80);
        uint32_t fg = wins[i].minimized ? RGB(140, 140, 140) : COL_WHITE;
        if (wins[i].minimized && i == active_win) bg = RGB(50, 50, 50);
        fb_fill_rect(bx, y + 2, bw, TASKBAR - 4, bg);
        fb_draw_string(bx + 4, y + (TASKBAR - 8) / 2, wins[i].title, fg, bg);
        bx += bw + 4;
    }

    {
        rtc_time_t now;
        rtc_read_time(&now);
        char time_str[12];
        time_str[0] = '0' + now.hours / 10;
        time_str[1] = '0' + now.hours % 10;
        time_str[2] = ':';
        time_str[3] = '0' + now.minutes / 10;
        time_str[4] = '0' + now.minutes % 10;
        time_str[5] = ':';
        time_str[6] = '0' + now.seconds / 10;
        time_str[7] = '0' + now.seconds % 10;
        time_str[8] = 0;
        int tw = 8 * 8;
        fb_draw_string(fb.width - tw - 4, y + (TASKBAR - 8) / 2, time_str, COL_WHITE, COL_TASKBAR);
    }
    draw_apps_menu();
}

static window_t *find_win(int x, int y) {
    for (int i = num_wins - 1; i >= 0; i--) {
        window_t *w = &wins[i];
        if (!w->visible) continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return w;
    }
    return 0;
}

static int is_in_title(window_t *w, int x, int y) {
    return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + TITLE_H;
}

static int is_in_close(window_t *w, int x, int y) {
    int cx = w->x + w->w - 18;
    int cy = w->y + (TITLE_H - 14) / 2;
    return x >= cx && x < cx + 14 && y >= cy && y < cy + 14;
}

static int is_in_minimize(window_t *w, int x, int y) {
    int mx = w->x + w->w - 34;
    int my = w->y + (TITLE_H - 14) / 2;
    return x >= mx && x < mx + 14 && y >= my && y < my + 14;
}

static int is_in_resize(window_t *w, int x, int y) {
    if (w->is_app) return 0;
    int rx = w->x + w->w - RESIZE_H;
    int ry = w->y + w->h - RESIZE_H;
    return x >= rx && x < rx + RESIZE_H && y >= ry && y < ry + RESIZE_H;
}

static void swap_wins(int a, int b) {
    window_t t;
    int i;
    for (i = 0; i < 32; i++) t.title[i] = wins[a].title[i];
    t.x = wins[a].x; t.y = wins[a].y; t.w = wins[a].w; t.h = wins[a].h;
    t.visible = wins[a].visible;
    t.minimized = wins[a].minimized;
    t.buf_row = wins[a].buf_row; t.buf_col = wins[a].buf_col;
    t.is_app = wins[a].is_app;
    t.redraw_fn = wins[a].redraw_fn;
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            t.buf[r][c] = wins[a].buf[r][c];

    for (i = 0; i < 32; i++) wins[a].title[i] = wins[b].title[i];
    wins[a].x = wins[b].x; wins[a].y = wins[b].y;
    wins[a].w = wins[b].w; wins[a].h = wins[b].h;
    wins[a].visible = wins[b].visible;
    wins[a].minimized = wins[b].minimized;
    wins[a].buf_row = wins[b].buf_row; wins[a].buf_col = wins[b].buf_col;
    wins[a].is_app = wins[b].is_app;
    wins[a].redraw_fn = wins[b].redraw_fn;
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            wins[a].buf[r][c] = wins[b].buf[r][c];

    for (i = 0; i < 32; i++) wins[b].title[i] = t.title[i];
    wins[b].x = t.x; wins[b].y = t.y; wins[b].w = t.w; wins[b].h = t.h;
    wins[b].visible = t.visible;
    wins[b].minimized = t.minimized;
    wins[b].buf_row = t.buf_row; wins[b].buf_col = t.buf_col;
    wins[b].is_app = t.is_app;
    wins[b].redraw_fn = t.redraw_fn;
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            wins[b].buf[r][c] = t.buf[r][c];
}

static void raise(window_t *w) {
    int idx = -1;
    for (int i = 0; i < num_wins; i++)
        if (&wins[i] == w) { idx = i; break; }
    if (idx < 0 || idx == num_wins - 1) return;
    for (int i = idx; i < num_wins - 1; i++)
        swap_wins(i, i + 1);
    active_win = num_wins - 1;
}

static void do_redraw(void) {
    if (cursor_visible) gui_cursor_hide();
    draw_background();
    for (int i = 0; i < num_wins; i++)
        draw_window(&wins[i]);
    draw_taskbar();
    if (drag_active) {
        int mx, my;
        mouse_get_pos(&mx, &my);
        int n = 0;
        while (drag_file[n]) n++;
        int tw = n * 8;
        int th = 10;
        int dx = mx + 8;
        int dy = my + 8;
        fb_fill_rect(dx, dy, tw + 6, th + 4, RGB(0, 80, 200));
        fb_draw_string(dx + 3, dy + 2, drag_file, COL_WHITE, RGB(0, 80, 200));
    }
    gui_cursor_show();
}

static void launch_app(int idx) {
    apps_menu_open = 0;
    do_redraw();
    if (idx == 0) {
        static int win_count = 1;
        char title[32];
        title[0] = 'T'; title[1] = 'e'; title[2] = 'r'; title[3] = 'm'; title[4] = ' ';
        int n = win_count;
        int pos = 5;
        if (n >= 100) { title[pos++] = '0' + n / 100; n %= 100; }
        if (n >= 10) { title[pos++] = '0' + n / 10; n %= 10; }
        title[pos++] = '0' + n;
        title[pos] = 0;
        win_count++;
        int wx = 40 + win_count * 20;
        int wy = 30 + win_count * 15;
        if (wx + 640 > fb.width) wx = fb.width - 640;
        if (wy + 440 > fb.height - 22) wy = fb.height - 22 - 440;
        gui_create_window(wx, wy, 640, 440, title);
        do_redraw();
    } else if (idx == 1) {
        paint_run();
        do_redraw();
    } else if (idx == 2) {
        snake_run();
        do_redraw();
    } else if (idx == 3) {
        browser_run();
        do_redraw();
    } else if (idx == 4) {
        settings_run();
        do_redraw();
    } else if (idx == 5) {
        files_run();
        do_redraw();
    } else if (idx == 6) {
        winver_run();
        do_redraw();
    } else if (idx == 7) {
        calcapp_run();
        do_redraw();
    } else if (idx == 8) {
        notepad_run();
        do_redraw();
    }
}

static int term_win_id(void) {
    if (active_win >= 0 && active_win < num_wins && !wins[active_win].is_app && wins[active_win].visible)
        return active_win;
    for (int i = num_wins - 1; i >= 0; i--)
        if (!wins[i].is_app && wins[i].visible)
            return i;
    return -1;
}

void gui_term_putchar(char c) {
    if (!gui_active) return;
    int tid = term_win_id();
    if (tid < 0) return;
    term_putchar(&wins[tid], c);
}

void gui_term_clear(void) {
    if (!gui_active) return;
    int tid = term_win_id();
    if (tid < 0) return;
    term_clear(&wins[tid]);
}

int gui_create_window(int x, int y, int w, int h, const char *title) {
    if (num_wins >= MAX_WINS) return -1;
    window_t *win = &wins[num_wins];
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    int i;
    for (i = 0; title[i] && i < 31; i++)
        win->title[i] = title[i];
    win->title[i] = 0;
    win->visible = 1;
    win->minimized = 0;
    win->is_app = 0;
    win->redraw_fn = 0;
    term_clear(win);
    num_wins++;
    active_win = num_wins - 1;
    return num_wins - 1;
}

void gui_mark_app_window(int id) {
    if (id >= 0 && id < num_wins)
        wins[id].is_app = 1;
}

void gui_window_set_redraw_fn(int id, window_draw_fn fn) {
    if (id >= 0 && id < num_wins) {
        wins[id].is_app = 1;
        wins[id].redraw_fn = fn;
    }
}

int gui_init(void) {
    gui_active = 0;
    num_wins = 0;
    active_win = -1;
    drag_win = -1;
    resize_win = -1;
    cursor_x = -1;
    cursor_y = -1;
    cursor_visible = 0;

    settings_load();

    int wx = (fb.width - 640) / 2;
    int wy = (fb.height - 440) / 3;

    gui_create_window(wx, wy, 640, 440, "Terminal");
    window_t *w = &wins[0];
    term_writestring(w, "\n");
    term_writestring(w, "  minios v0.1 GUI - UNIX-like OS\n");
    term_writestring(w, "  ==============================\n");
    term_writestring(w, "\n");
    term_writestring(w, "  Type 'help' for commands.\n");
    term_writestring(w, "\n");

    gui_active = 1;
    return 1;
}

void gui_cursor_hide(void) {
    if (cursor_x >= 0) cursor_xor_at(cursor_x, cursor_y);
    cursor_x = -1;
    cursor_y = -1;
    cursor_visible = 0;
}

void gui_cursor_show(void) {
    int mx, my;
    mouse_get_pos(&mx, &my);
    cursor_xor_at(mx, my);
    cursor_x = mx;
    cursor_y = my;
    cursor_visible = 1;
}

int gui_active_win(void) {
    return active_win;
}

void gui_set_window_title(int id, const char *title) {
    if (id < 0 || id >= num_wins) return;
    int i;
    for (i = 0; title[i] && i < 31; i++)
        wins[id].title[i] = title[i];
    wins[id].title[i] = 0;
}

void gui_get_body(int id, int *x, int *y, int *w, int *h) {
    if (id < 0 || id >= num_wins) { *x = *y = *w = *h = 0; return; }
    *x = wins[id].x;
    *y = wins[id].y + TITLE_H;
    *w = wins[id].w;
    *h = wins[id].h - TITLE_H;
}

void gui_redraw(void) {
    if (!gui_active) return;
    do_redraw();
}

void gui_drag_start(const char *name) {
    int i;
    for (i = 0; name[i] && i < FS_NAME_LEN - 1; i++)
        drag_file[i] = name[i];
    drag_file[i] = 0;
    drag_active = 1;
    drag_from_win = -1;
}

const char *gui_drag_file(void) {
    return drag_active ? drag_file : 0;
}

void gui_poll(void) {
    if (!gui_active) return;

    mouse_poll();

    static int prev_buttons = 0;
    int btns = mouse_get_buttons();
    int mx, my;
    mouse_get_pos(&mx, &my);

    if ((btns & 1) && !(prev_buttons & 1)) {
        int tb_y = fb.height - TASKBAR;
        int my2 = tb_y - APP_COUNT * APP_MENU_ITEM_H - 4;

        if (apps_menu_open && mx >= 5 && mx < 5 + APP_MENU_W && my >= my2 && my < tb_y) {
            int item = (my - my2 - 2) / APP_MENU_ITEM_H;
            if (item >= 0 && item < APP_COUNT)
                launch_app(item);
        } else if (my >= tb_y) {
            if (mx >= 4 && mx < 4 + APP_BTN_W && my >= tb_y + 2 && my < tb_y + TASKBAR - 2) {
                apps_menu_open = !apps_menu_open;
                do_redraw();
            } else {
                if (apps_menu_open) { apps_menu_open = 0; do_redraw(); }
                int bx = 4 + APP_BTN_W + 6;
                for (int i = 0; i < num_wins; i++) {
                    if (!wins[i].visible && !wins[i].minimized) continue;
                    int bw = 100;
                    if (mx >= bx && mx < bx + bw) {
                        if (wins[i].minimized) {
                            wins[i].visible = 1;
                            wins[i].minimized = 0;
                        }
                        raise(&wins[i]);
                        do_redraw();
                        break;
                    }
                    bx += bw + 4;
                }
            }
        } else {
            if (apps_menu_open) {
                apps_menu_open = 0;
                do_redraw();
            }
            window_t *w = find_win(mx, my);
            if (w) {
                raise(w);
                if (is_in_close(w, mx, my)) {
                    w->visible = 0;
                    for (int i = num_wins - 1; i >= 0; i--)
                        if (wins[i].visible) { raise(&wins[i]); break; }
                    do_redraw();
                } else if (is_in_minimize(w, mx, my)) {
                    w->visible = 0;
                    w->minimized = 1;
                    for (int i = num_wins - 1; i >= 0; i--)
                        if (wins[i].visible) { raise(&wins[i]); break; }
                    do_redraw();
                } else if (is_in_resize(w, mx, my)) {
                    resize_win = -1;
                    for (int i = 0; i < num_wins; i++)
                        if (&wins[i] == w) { resize_win = i; break; }
                    resize_ox = mx;
                    resize_oy = my;
                    resize_ow = w->w;
                    resize_oh = w->h;
                } else if (is_in_title(w, mx, my)) {
                    drag_win = -1;
                    for (int i = 0; i < num_wins; i++)
                        if (&wins[i] == w) { drag_win = i; break; }
                    drag_ox = mx - w->x;
                    drag_oy = my - w->y;
                }
            }
        }
    }

    if (!(btns & 1)) {
        if (drag_win >= 0) {
            drag_win = -1;
            do_redraw();
        }
        if (resize_win >= 0) {
            resize_win = -1;
            do_redraw();
        }
        if (drag_active) {
            window_t *target = find_win(mx, my);
            if (target) {
                if (!target->is_app) {
                    extern void shell_inject(const char *cmd);
                    char cmd[64];
                    int ci = 0;
                    cmd[ci++] = 'c'; cmd[ci++] = 'a'; cmd[ci++] = 't'; cmd[ci++] = ' ';
                    int fi = 0;
                    while (drag_file[fi] && ci < 63) cmd[ci++] = drag_file[fi++];
                    cmd[ci] = 0;
                    shell_inject(cmd);
                } else {
                    const char *t = target->title;
                    if (t[0]=='N' && t[1]=='o' && t[2]=='t' && t[3]=='e' && t[4]=='p' && t[5]=='a' && t[6]=='d')
                        notepad_open(drag_file);
                }
            }
            drag_active = 0;
            drag_file[0] = 0;
            do_redraw();
        }
    }

    if (drag_win >= 0) {
        wins[drag_win].x = mx - drag_ox;
        wins[drag_win].y = my - drag_oy;
        if (wins[drag_win].x < 0) wins[drag_win].x = 0;
        if (wins[drag_win].y < 0) wins[drag_win].y = 0;
        do_redraw();
    }

    if (resize_win >= 0) {
        int dw = mx - resize_ox;
        int dh = my - resize_oy;
        int nw = resize_ow + dw;
        int nh = resize_oh + dh;
        if (nw < 160) nw = 160;
        if (nh < 120) nh = 120;
        wins[resize_win].w = nw;
        wins[resize_win].h = nh;
        do_redraw();
    }

    {
        static int clock_tick;
        clock_tick++;
        if (clock_tick >= 100) {
            clock_tick = 0;
            do_redraw();
        }
    }

    prev_buttons = btns;
    cursor_update();
}
