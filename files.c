#include "files.h"
#include "gui.h"
#include "framebuffer.h"
#include "mouse.h"
#include "keyboard.h"
#include "fs.h"

#define BTN_H 20
#define LIST_ITEM_H 16

static int files_win;
static int f_sel, f_scroll;
static int f_editing;

static int in_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static void draw_btn(int x, int y, int w, int h, const char *label, uint32_t bg) {
    fb_fill_rect(x, y, w, h, bg);
    fb_draw_string(x + 4, y + (h - 8) / 2, label, RGB(255, 255, 255), bg);
}

static void strcp(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
}

static void files_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    int list_y = by + 2;
    int list_h = bh - BTN_H - 8;
    fb_fill_rect(bx + 1, list_y, bw - 2, list_h, RGB(255, 255, 255));

    char fnames[FS_MAX_FILES][FS_NAME_LEN];
    int num_files = fs_list(fnames, FS_MAX_FILES);
    int visible_rows = list_h / LIST_ITEM_H;

    for (int i = f_scroll; i < num_files && i < f_scroll + visible_rows; i++) {
        int iy = list_y + (i - f_scroll) * LIST_ITEM_H;
        uint32_t bg = (i == f_sel) ? RGB(200, 220, 255) : RGB(255, 255, 255);
        fb_fill_rect(bx + 1, iy, bw - 2, LIST_ITEM_H - 1, bg);
        fb_draw_string(bx + 8, iy + 3, fnames[i], RGB(0, 0, 0), bg);
    }
}

void files_run(void) {
    int ww = 480, wh = 380;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    files_win = gui_create_window(wx, wy, ww, wh, "File Manager");
    if (files_win < 0) return;
    gui_mark_app_window(files_win);
    gui_window_set_redraw_fn(files_win, files_redraw_fn);
    gui_redraw();

    int bx, by, bw, bh;
    gui_get_body(files_win, &bx, &by, &bw, &bh);

    f_sel = -1;
    f_scroll = 0;
    f_editing = 0;

    char fnames[FS_MAX_FILES][FS_NAME_LEN];
    int num_files;

    char edit_buf[4096];
    int edit_len = 0;
    int edit_cursor = 0;
    char edit_name[FS_NAME_LEN];

    int prev_btn = 0;
    int running = 1;

    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();

        int list_y = by + 2;
        int list_h = bh - BTN_H - 8;
        int btn_y = by + bh - BTN_H - 4;

        int btn_w = 60;
        int total_w = btn_w * 6 + 5 * 4;
        int btn_start = bx + (bw - total_w) / 2;
        int new_x = btn_start;
        int edit_x = new_x + btn_w + 4;
        int ren_x = edit_x + btn_w + 4;
        int del_x = ren_x + btn_w + 4;
        int run_x = del_x + btn_w + 4;
        int ref_x = run_x + btn_w + 4;

        if (!f_editing) {
            int mx, my;
            mouse_get_pos(&mx, &my);
            int btns = mouse_get_buttons();

            num_files = fs_list(fnames, FS_MAX_FILES);

            if ((btns & 1) && !(prev_btn & 1)) {
                if (in_rect(mx, my, new_x, btn_y, btn_w, BTN_H)) {
                    char newname[FS_NAME_LEN];
                    int np = 0;
                    for (int i = 0; i < FS_NAME_LEN; i++) newname[i] = 0;
                    while (1) {
                        gui_poll();
                        keyboard_poll();
                        char k = keyboard_getchar();
                        if (k == 27) break;
                        if (k == '\n' && np > 0) {
                            newname[np] = 0;
                            fs_create(newname);
                            break;
                        }
                        if (k == '\b' && np > 0) { np--; newname[np] = 0; }
                        else if (k >= ' ' && np < FS_NAME_LEN - 1) newname[np++] = k;
                        fb_fill_rect(bx + 1, list_y, bw - 2, list_h, RGB(255, 255, 255));
                        fb_draw_string(bx + 8, list_y + 4, "New file name: ", RGB(0, 0, 0), RGB(255, 255, 255));
                        fb_draw_string(bx + 8 + 15 * 8, list_y + 4, newname, RGB(0, 0, 180), RGB(255, 255, 255));
                    }
                } else if (in_rect(mx, my, edit_x, btn_y, btn_w, BTN_H)) {
                    if (f_sel >= 0 && f_sel < num_files) {
                        f_editing = 1;
                        strcp(edit_name, fnames[f_sel]);
                        edit_len = 0;
                        int r = fs_read(fnames[f_sel], (uint8_t *)edit_buf, 4095);
                        if (r > 0) edit_len = r;
                        edit_buf[edit_len] = 0;
                        edit_cursor = edit_len;
                    }
                } else if (in_rect(mx, my, ren_x, btn_y, btn_w, BTN_H)) {
                    if (f_sel >= 0 && f_sel < num_files) {
                        char newname[FS_NAME_LEN];
                        int np = 0;
                        for (int i = 0; i < FS_NAME_LEN; i++) newname[i] = 0;
                        while (1) {
                            gui_poll();
                            keyboard_poll();
                            char k = keyboard_getchar();
                            if (k == 27) break;
                            if (k == '\n' && np > 0) {
                                newname[np] = 0;
                                fs_rename(fnames[f_sel], newname);
                                break;
                            }
                            if (k == '\b' && np > 0) { np--; newname[np] = 0; }
                            else if (k >= ' ' && np < FS_NAME_LEN - 1) newname[np++] = k;
                            fb_fill_rect(bx + 1, list_y, bw - 2, list_h, RGB(255, 255, 255));
                            fb_draw_string(bx + 8, list_y + 4, "Rename to: ", RGB(0, 0, 0), RGB(255, 255, 255));
                            fb_draw_string(bx + 8 + 11 * 8, list_y + 4, newname, RGB(0, 0, 180), RGB(255, 255, 255));
                        }
                    }
                } else if (in_rect(mx, my, del_x, btn_y, btn_w, BTN_H)) {
                    if (f_sel >= 0 && f_sel < num_files) { fs_delete(fnames[f_sel]); f_sel = -1; }
                } else if (in_rect(mx, my, run_x, btn_y, btn_w, BTN_H)) {
                    if (f_sel >= 0 && f_sel < num_files) fs_run(fnames[f_sel]);
                } else if (in_rect(mx, my, ref_x, btn_y, btn_w, BTN_H)) {
                } else if (mx >= bx + 1 && mx < bx + bw - 1 && my >= list_y && my < list_y + list_h) {
                    int idx = (my - list_y) / LIST_ITEM_H + f_scroll;
                    if (idx >= 0 && idx < num_files) {
                        if (f_sel == idx)
                            gui_drag_start(fnames[idx]);
                        f_sel = idx;
                    }
                }
            }
            prev_btn = btns;

            int visible_rows = list_h / LIST_ITEM_H;
            if (f_scroll > num_files - visible_rows && f_scroll > 0) f_scroll = num_files - visible_rows;
            if (f_scroll < 0) f_scroll = 0;

            if (c == 'w' || c == 'W') { f_sel--; if (f_sel < 0) f_sel = 0; if (f_sel < f_scroll) f_scroll = f_sel; }
            if (c == 's' || c == 'S') { f_sel++; if (f_sel >= num_files) f_sel = num_files - 1; if (f_sel >= f_scroll + visible_rows) f_scroll = f_sel - visible_rows + 1; }

            fb_fill_rect(bx + 1, list_y, bw - 2, list_h, RGB(255, 255, 255));
            for (int i = f_scroll; i < num_files && i < f_scroll + visible_rows; i++) {
                int iy = list_y + (i - f_scroll) * LIST_ITEM_H;
                uint32_t bg = (i == f_sel) ? RGB(200, 220, 255) : RGB(255, 255, 255);
                fb_fill_rect(bx + 1, iy, bw - 2, LIST_ITEM_H - 1, bg);
                fb_draw_string(bx + 8, iy + 3, fnames[i], RGB(0, 0, 0), bg);
            }

            draw_btn(new_x, btn_y, btn_w, BTN_H, "New", RGB(0, 100, 0));
            draw_btn(edit_x, btn_y, btn_w, BTN_H, "Edit", RGB(0, 60, 180));
            draw_btn(ren_x, btn_y, btn_w, BTN_H, "Rename", RGB(120, 80, 0));
            draw_btn(del_x, btn_y, btn_w, BTN_H, "Delete", RGB(180, 0, 0));
            draw_btn(run_x, btn_y, btn_w, BTN_H, "Run", RGB(100, 0, 100));
            draw_btn(ref_x, btn_y, btn_w, BTN_H, "Refresh", RGB(80, 80, 80));

        } else {
            int mx, my;
            mouse_get_pos(&mx, &my);
            int btns = mouse_get_buttons();

            fb_fill_rect(bx + 1, list_y, bw - 2, list_h, RGB(255, 255, 255));
            fb_draw_string(bx + 8, list_y + 2, "Editing: ", RGB(0, 0, 180), RGB(255, 255, 255));
            fb_draw_string(bx + 8 + 9 * 8, list_y + 2, edit_name, RGB(0, 0, 0), RGB(255, 255, 255));

            int text_y = list_y + 16;
            int text_h = list_h - 16;
            int text_w = bw - 4;
            int max_lines = text_h / 10;
            if (max_lines < 1) max_lines = 1;

            if (c == 27) { f_editing = 0; }
            else if (c == '\b') {
                if (edit_cursor > 0) {
                    for (int i = edit_cursor - 1; i < edit_len; i++) edit_buf[i] = edit_buf[i + 1];
                    edit_len--; edit_cursor--;
                }
            } else if (c == '\n') {
                if (edit_len < 4095) {
                    for (int i = edit_len; i > edit_cursor; i--) edit_buf[i] = edit_buf[i - 1];
                    edit_buf[edit_cursor] = '\n'; edit_len++; edit_cursor++;
                }
            } else if (c >= ' ' && edit_len < 4095) {
                for (int i = edit_len; i > edit_cursor; i--) edit_buf[i] = edit_buf[i - 1];
                edit_buf[edit_cursor] = c; edit_len++; edit_cursor++;
            }

            edit_buf[edit_len] = 0;
            int line_start = 0;
            int cursor_line = -1, cursor_col = -1;
            for (int li = 0; li < max_lines && line_start <= edit_len; li++) {
                int end = line_start;
                while (end < edit_len && edit_buf[end] != '\n' && (end - line_start) < text_w / 8)
                    end++;
                if (end >= edit_len || edit_buf[end] == '\n') {
                    char linebuf[256];
                    int len = end - line_start;
                    int j;
                    for (j = 0; j < len && j < 255; j++) linebuf[j] = edit_buf[line_start + j];
                    linebuf[j] = 0;
                    fb_draw_string(bx + 4, text_y + li * 10, linebuf, RGB(0, 0, 0), RGB(255, 255, 255));
                    if (edit_cursor >= line_start && edit_cursor <= line_start + len) {
                        cursor_line = li;
                        cursor_col = (edit_cursor - line_start) * 8;
                    }
                    if (end < edit_len && edit_buf[end] == '\n') {
                        if (edit_cursor == end) { cursor_line = li; cursor_col = len * 8; }
                        line_start = end + 1;
                    } else {
                        line_start = end;
                    }
                }
            }

            if (cursor_line >= 0) {
                int cx = bx + 4 + cursor_col;
                int cy = text_y + cursor_line * 10;
                fb_fill_rect(cx, cy, 2, 9, RGB(0, 0, 180));
            }

            int sb_w = 70;
            int sb_y = btn_y;
            int save_x = bx + (bw - sb_w * 2 - 10) / 2;
            int cancel_x = save_x + sb_w + 10;

            draw_btn(save_x, sb_y, sb_w, BTN_H, "Save", RGB(0, 100, 0));
            draw_btn(cancel_x, sb_y, sb_w, BTN_H, "Cancel", RGB(120, 0, 0));

            if ((btns & 1) && !(prev_btn & 1)) {
                if (in_rect(mx, my, save_x, sb_y, sb_w, BTN_H)) {
                    edit_buf[edit_len] = 0;
                    fs_write(edit_name, (uint8_t *)edit_buf, edit_len);
                    f_editing = 0;
                } else if (in_rect(mx, my, cancel_x, sb_y, sb_w, BTN_H)) {
                    f_editing = 0;
                }
            }
            prev_btn = btns;
        }

        if (!f_editing && c == 27) running = 0;
    }

    gui_redraw();
}