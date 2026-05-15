#include "settings.h"
#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"
#include "rtc.h"
#include "rtl8139.h"
#include "fs.h"
#include "settings_store.h"

#define C_TEXT  RGB(0, 0, 0)
#define C_GRAY  RGB(100, 100, 100)
#define C_WHITE RGB(255, 255, 255)
#define C_BLUE  RGB(0, 0, 180)
#define C_SECTION RGB(0, 60, 120)
#define C_HL    RGB(220, 220, 255)
#define C_BTN   RGB(0, 80, 180)
#define C_BTN_T RGB(255, 255, 255)

static int opt_mouse_y[2], opt_bg_y[2], opt_de_y[2], opt_anim_y[2], opt_spd_y[2];
static int opt_save_y;
static int bx, bw;

static const char *icon_names[] = {"Crosshair", "Arrow", "Hand"};
static const char *bg_names[] = {"Checkerboard", "Solid", "Stripes"};
static const char *de_names[] = {"Default DE", "Pixel DE"};

static void print_val(int x, int *y, const char *label, const char *val) {
    fb_draw_string(x, *y, label, C_GRAY, C_WHITE);
    fb_draw_string(x + 120, *y, val, C_TEXT, C_WHITE);
    *y += 14;
}

static void print_opt(int x, int *y, const char *label, const char *val, int hilite, int y_out[2]) {
    uint32_t bg = hilite ? C_HL : C_WHITE;
    fb_fill_rect(x, *y, bw, 14, bg);
    fb_draw_string(x, *y, label, C_TEXT, bg);
    fb_draw_string(x + 130, *y, val, C_BLUE, bg);
    if (y_out) { y_out[0] = *y; y_out[1] = *y + 14; }
    *y += 16;
}

static void settings_redraw_fn(int id) {
    int bx2, by, bw2, bh;
    gui_get_body(id, &bx2, &by, &bw2, &bh);
    bx = bx2 + 8;
    bw = bw2 - 16;
    fb_fill_rect(bx2 + 1, by + 1, bw2 - 2, bh - 2, C_WHITE);

    int x = bx;
    int y = by + 8;

    fb_draw_string(x, y, "minios Settings", C_BLUE, C_WHITE); y += 20;

    // Mouse cursor
    fb_draw_string(x, y, "Mouse Cursor", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    print_opt(x, &y, "Icon:", icon_names[gui_settings.mouse_icon % 3], 0, opt_mouse_y);

    // Background
    fb_draw_string(x, y, "Desktop Background", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    int bp = gui_settings.bg_pattern;
    if (bp < 0 || bp > 2) bp = 0;
    print_opt(x, &y, "Pattern:", bg_names[bp], 0, opt_bg_y);

    // Desktop Environment
    fb_draw_string(x, y, "Desktop Env", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    int de = gui_settings.desktop_env;
    if (de < 0 || de > 1) de = 0;
    print_opt(x, &y, "Mode:", de_names[de], 0, opt_de_y);

    // Boot animation
    fb_draw_string(x, y, "Boot Animation", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    print_opt(x, &y, "Enabled:", gui_settings.boot_anim ? "Yes" : "No", 0, opt_anim_y);

    // Animation speed
    fb_draw_string(x, y, "Anim Speed", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    char spd_str[4];
    spd_str[0] = '0' + gui_settings.anim_speed;
    spd_str[1] = 0;
    print_opt(x, &y, "Speed:", spd_str, 0, opt_spd_y);

    // Save button
    y += 8;
    opt_save_y = y;
    fb_fill_rect(x, y, 120, 22, C_BTN);
    fb_draw_string(x + 20, y + (22 - 8) / 2, "Save Settings", C_BTN_T, C_BTN);
    y += 30;

    // System info (read-only at bottom)
    fb_draw_string(x, y, "System", C_SECTION, C_WHITE); y += 14;
    fb_fill_rect(x, y - 2, bw, 1, C_SECTION);
    print_val(x, &y, "OS:", "minios v0.1");
    print_val(x, &y, "Arch:", "i686");

    y += 4;
    fb_draw_string(x, y, "ESC - Close", C_GRAY, C_WHITE);
}

void settings_run(void) {
    int ww = 340, wh = 450;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    int win = gui_create_window(wx, wy, ww, wh, "Settings");
    if (win < 0) return;
    gui_mark_app_window(win);
    gui_window_set_redraw_fn(win, settings_redraw_fn);

    gui_redraw();

    int running = 1;
    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();
        if (c == 27) running = 0;

        int mx, my, btns;
        mouse_get_pos(&mx, &my);
        btns = mouse_get_buttons();
        if ((btns & 1) && !(btns & 2)) {
            int my_click = my;
            if (my_click >= opt_mouse_y[0] && my_click < opt_mouse_y[1] && mx >= bx && mx < bx + bw) {
                gui_settings.mouse_icon = (gui_settings.mouse_icon + 1) % 3;
                gui_redraw();
            } else if (my_click >= opt_bg_y[0] && my_click < opt_bg_y[1] && mx >= bx && mx < bx + bw) {
                gui_settings.bg_pattern = (gui_settings.bg_pattern + 1) % 3;
                gui_redraw();
            } else if (my_click >= opt_de_y[0] && my_click < opt_de_y[1] && mx >= bx && mx < bx + bw) {
                gui_settings.desktop_env = (gui_settings.desktop_env + 1) % 2;
                gui_redraw();
            } else if (my_click >= opt_anim_y[0] && my_click < opt_anim_y[1] && mx >= bx && mx < bx + bw) {
                gui_settings.boot_anim = !gui_settings.boot_anim;
                gui_redraw();
            } else if (my_click >= opt_spd_y[0] && my_click < opt_spd_y[1] && mx >= bx && mx < bx + bw) {
                gui_settings.anim_speed = (gui_settings.anim_speed % 3) + 1;
                gui_redraw();
            } else if (my_click >= opt_save_y && my_click < opt_save_y + 22 && mx >= bx && mx < bx + 120) {
                settings_save();
                gui_redraw();
            }
        }
    }

    gui_redraw();
}
