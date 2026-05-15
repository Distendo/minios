#include "browser.h"
#include "gui.h"
#include "framebuffer.h"
#include "mouse.h"
#include "keyboard.h"
#include "ports.h"
#include "net.h"

#define C_TEXT     RGB(0, 0, 0)
#define C_WHITE    RGB(255, 255, 255)

#define ADDR_BAR_H 20
#define BTN_W 40
#define BTN_H 16

static const char *page_welcome[] = {
    "  Welcome to minios 0.1",
    "",
    "  A minimal UNIX-like OS for x86.",
    "  Graphical UI with VBE framebuffer.",
    "",
    "  Enter a URL in the address bar:",
    "    welcome://  - This page",
    "    help://     - Help & commands",
    "    about://    - About minios",
    "    http://     - HTTP websites",
    0
};

static const char *page_help[] = {
    "  Help - Available Commands",
    "",
    "  Shell commands:",
    "    help     Show this help",
    "    clear    Clear terminal",
    "    echo     Echo text",
    "    uname    System info",
    "    window   New terminal",
    "    calc     RPN calculator",
    "    paint    Drawing app",
    "    snake    Snake game",
    "    browser  Web browser",
    "",
    "  GUI tips:",
    "    - Click title bar to drag windows",
    "    - Click X to close window",
    "    - Click taskbar to raise windows",
    "    - Click Apps for program menu",
    0
};

static const char *page_about[] = {
    "  About minios",
    "",
    "  Version: 0.1",
    "  Architecture: i686",
    "  License: MIT",
    "",
    "  Built with:",
    "    GCC cross-compiler",
    "    NASM assembler",
    "    QEMU emulator",
    "",
    "  Components:",
    "    Multiboot-compliant boot",
    "    PS/2 keyboard & mouse",
    "    Bochs VBE framebuffer",
    "    Window manager",
    "    Terminal with shell",
    0
};

static const char *page_notfound[] = {
    "  Page Not Found",
    "",
    "  The requested page could not be loaded.",
    "  Please check the URL and try again.",
    0
};

static const char *page_blank[] = {
    "  Enter a URL above and press Enter.",
    0
};

static const char *page_http_result[] = {
    "  HTTP response received.",
    "  (content displayed below)",
    0
};

static const char **pages[] = {page_welcome, page_help, page_about, page_notfound, page_blank, page_http_result};
static const char *page_urls[] = {"welcome://", "help://", "about://", "", "", ""};

#define NUM_PAGES 3

static int find_page(const char *url) {
    for (int i = 0; i < NUM_PAGES; i++)
        if (url[0] == page_urls[i][0]) {
            int j = 0;
            while (url[j] && page_urls[i][j] && url[j] == page_urls[i][j]) j++;
            if (url[j] == 0 && page_urls[i][j] == 0) return i;
        }
    // check for http://
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p') return 5;
    return 3;
}

static void draw_page(int bx, int by, int bw, int bh, int page_idx) {
    fb_fill_rect(bx, by, bw, bh, C_WHITE);
    const char **lines = pages[page_idx];
    int y = by + 4;
    for (int i = 0; lines[i]; i++) {
        const char *line = lines[i];
        int is_header = 0;
        if (line[0] == ' ' && line[1] != ' ') is_header = 1;
        if (line[0] == 'H' && line[1] == 'e') is_header = 1;
        uint32_t col = is_header ? RGB(0, 0, 180) : C_TEXT;
        fb_draw_string(bx + 4, y, line, col, C_WHITE);
        y += 10;
        if (y + 10 > by + bh) break;
    }
}

static int browser_win;
static int b_addr_y, b_content_y, b_content_h;
static char http_resp[4096];
static int http_resp_len;

static void browser_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    fb_fill_rect(bx + 1, by + 2, bw - 2, 20, C_WHITE);
    fb_fill_rect(bx + 4, by + 4, bw - 76, 16, C_WHITE);
    fb_draw_string(bx + 6, by + 6, "", C_TEXT, C_WHITE);
    if (http_resp_len > 0) {
        fb_fill_rect(bx + 1, b_content_y, bw - 2, b_content_h, C_WHITE);
        int y = b_content_y + 4;
        for (int i = 0; i < http_resp_len && y < b_content_y + b_content_h - 10; ) {
            char line[128];
            int li = 0;
            while (i < http_resp_len && http_resp[i] != '\n' && li < 127)
                line[li++] = http_resp[i++];
            line[li] = 0;
            if (i < http_resp_len && http_resp[i] == '\n') i++;
            fb_draw_string(bx + 4, y, line, C_TEXT, C_WHITE);
            y += 10;
        }
    }
}

void browser_run(void) {
    int ww = 640, wh = 440;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    browser_win = gui_create_window(wx, wy, ww, wh, "Browser");
    if (browser_win < 0) return;
    gui_mark_app_window(browser_win);
    gui_window_set_redraw_fn(browser_win, browser_redraw_fn);

    int bx, by, bw, bh;
    gui_get_body(browser_win, &bx, &by, &bw, &bh);

    b_addr_y = by + 2;
    b_content_y = b_addr_y + ADDR_BAR_H + 2;
    b_content_h = bh - ADDR_BAR_H - 6;
    http_resp_len = 0;

    char addr_buf[64];
    int addr_pos = 0;
    for (int i = 0; i < 64; i++) addr_buf[i] = 0;

    int cur_page = 0;
    int running = 1;
    int addr_active = 0;

    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();

        if (c == 27) running = 0;

        fb_fill_rect(bx + 1, b_addr_y, bw - 2, ADDR_BAR_H, C_WHITE);
        uint32_t addr_bg = addr_active ? RGB(255, 255, 200) : C_WHITE;
        fb_fill_rect(bx + 4, b_addr_y + 2, bw - 76, ADDR_BAR_H - 4, addr_bg);
        fb_draw_string(bx + 6, b_addr_y + 4, addr_buf, C_TEXT, addr_bg);

        fb_fill_rect(bx + bw - 68, b_addr_y + 2, BTN_W, BTN_H, RGB(80, 80, 80));
        fb_draw_string(bx + bw - 66, b_addr_y + 4, "Home", C_WHITE, RGB(80, 80, 80));
        fb_fill_rect(bx + bw - 24, b_addr_y + 2, 20, BTN_H, RGB(0, 100, 0));
        fb_draw_string(bx + bw - 23, b_addr_y + 4, "Go", C_WHITE, RGB(0, 100, 0));

        int navigate = 0;
        if (c == '\n' && addr_active) { navigate = 1; }
        else if (c == '\t') { addr_active = !addr_active; }
        else if (c == '\b') { if (addr_pos > 0) { addr_pos--; addr_buf[addr_pos] = 0; } }
        else if (c >= ' ' && addr_active && addr_pos < 63) {
            addr_buf[addr_pos++] = c;
            addr_buf[addr_pos] = 0;
        }

        int mx, my;
        mouse_get_pos(&mx, &my);
        int btns = mouse_get_buttons();

        static int prev_btn = 0;
        if ((btns & 1) && !(prev_btn & 1)) {
            if (mx >= bx + 4 && mx < bx + bw - 72 && my >= b_addr_y + 2 && my < b_addr_y + ADDR_BAR_H - 2) {
                addr_active = 1;
            } else if (mx >= bx + bw - 68 && mx < bx + bw - 68 + BTN_W && my >= b_addr_y + 2 && my < b_addr_y + 2 + BTN_H) {
                addr_active = 0;
                addr_pos = 0;
                addr_buf[0] = 0;
                cur_page = 0;
                navigate = 1; // navigate to welcome page
            } else if (mx >= bx + bw - 24 && mx < bx + bw - 4 && my >= b_addr_y + 2 && my < b_addr_y + 2 + BTN_H) {
                navigate = 1;
            } else {
                addr_active = 0;
            }
        }
        prev_btn = btns;

        if (navigate && addr_pos > 0) {
            addr_buf[addr_pos] = 0;
            cur_page = find_page(addr_buf);
            if (cur_page == 5) {
                // HTTP request
                fb_fill_rect(bx + 1, b_content_y, bw - 2, b_content_h, C_WHITE);
                fb_draw_string(bx + 8, b_content_y + 4, "Loading...", RGB(0, 0, 180), C_WHITE);
                // Parse URL: http://host[:port]/path
                char host[128];
                char path[256];
                int port = 80;
                int hp = 0, pp = 0;
                const char *u = addr_buf + 7; // skip "http://"
                // read host
                while (*u && *u != '/' && *u != ':' && hp < 127) host[hp++] = *u++;
                host[hp] = 0;
                if (*u == ':') {
                    u++;
                    port = 0;
                    while (*u >= '0' && *u <= '9') { port = port * 10 + (*u - '0'); u++; }
                }
                // read path
                if (*u == '/') {
                    while (*u && pp < 255) path[pp++] = *u++;
                }
                path[pp] = 0;
                if (pp == 0) { path[0] = '/'; path[1] = 0; }

                http_resp_len = net_http_get(host, port, path, http_resp, 4095);
                if (http_resp_len > 0) {
                    http_resp[http_resp_len] = 0;
                    fb_fill_rect(bx + 1, b_content_y, bw - 2, b_content_h, C_WHITE);
                    int y = b_content_y + 4;
                    for (int i = 0; i < http_resp_len && y < b_content_y + b_content_h - 10; ) {
                        char line[128];
                        int li = 0;
                        while (i < http_resp_len && http_resp[i] != '\n' && li < 127)
                            line[li++] = http_resp[i++];
                        line[li] = 0;
                        if (i < http_resp_len && http_resp[i] == '\n') i++;
                        // skip HTTP headers (stop at blank line)
                        if (line[0] == '\r' && line[1] == 0) {
                            // headers done, remaining is body
                            continue;
                        }
                        // simple header filter
                        int show = 1;
                        for (int si = 0; line[si]; si++) {
                            if (line[si] == '<') { show = 0; break; }
                            if (line[si] == '&') { show = 0; break; }
                            if (line[si] > 126) { show = 0; break; }
                        }
                        if (show && li > 0 && line[0] != '\r') {
                            fb_draw_string(bx + 4, y, line, C_TEXT, C_WHITE);
                            y += 10;
                        }
                    }
                } else {
                    fb_fill_rect(bx + 1, b_content_y, bw - 2, b_content_h, C_WHITE);
                    fb_draw_string(bx + 8, b_content_y + 4, "HTTP request failed", RGB(180, 0, 0), C_WHITE);
                    cur_page = 3;
                }
            } else {
                draw_page(bx + 1, b_content_y, bw - 2, b_content_h, cur_page);
            }
        }

        if (cur_page >= 0 && cur_page < 4)
            draw_page(bx + 1, b_content_y, bw - 2, b_content_h, cur_page);
        else if (cur_page == 4)
            draw_page(bx + 1, b_content_y, bw - 2, b_content_h, 0);
        else if (cur_page == 5 && http_resp_len == 0)
            draw_page(bx + 1, b_content_y, bw - 2, b_content_h, 0);
        else if (cur_page == 5 && http_resp_len > 0) {
            // already drawn above
        } else
            draw_page(bx + 1, b_content_y, bw - 2, b_content_h, 3);
    }

    gui_redraw();
}