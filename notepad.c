#include "notepad.h"
#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "fs.h"

#define C_BG    RGB(255, 255, 255)
#define C_TEXT  RGB(0, 0, 0)
#define C_STAT  RGB(240, 240, 240)
#define C_CURS  RGB(0, 0, 200)

#define EDIT_ROWS 30
#define EDIT_COLS 60

static char edit_buf[EDIT_ROWS][EDIT_COLS];
static int cur_row, cur_col;
static char open_fname[32];
static int dirty;

static void edit_clear(void) {
    for (int r = 0; r < EDIT_ROWS; r++)
        for (int c = 0; c < EDIT_COLS; c++)
            edit_buf[r][c] = ' ';
    cur_row = 0;
    cur_col = 0;
    dirty = 0;
}

static void edit_load(const char *fname) {
    edit_clear();
    uint8_t raw[4096];
    int sz = fs_read(fname, raw, 4095);
    if (sz <= 0) return;
    raw[sz] = 0;
    int r = 0, c = 0;
    for (int i = 0; i < sz && r < EDIT_ROWS; i++) {
        char ch = raw[i];
        if (ch == '\n') { r++; c = 0; }
        else if (ch >= ' ' && c < EDIT_COLS - 1) { edit_buf[r][c++] = ch; }
    }
    cur_row = 0;
    cur_col = 0;
    dirty = 0;
}

static void edit_save(void) {
    char out[4096];
    int oi = 0;
    for (int r = 0; r < EDIT_ROWS && oi < 4095; r++) {
        int end = EDIT_COLS - 1;
        while (end > 0 && edit_buf[r][end] == ' ') end--;
        for (int c = 0; c <= end && oi < 4095; c++)
            out[oi++] = edit_buf[r][c];
        if (oi < 4095) out[oi++] = '\n';
    }
    out[oi] = 0;
    fs_write(open_fname, (uint8_t *)out, oi);
    dirty = 0;
}

static void notepad_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    fb_fill_rect(bx + 1, by + 1, bw - 2, bh - 2, C_BG);

    int rows = (bh - 22) / 14;
    if (rows > EDIT_ROWS) rows = EDIT_ROWS;
    int cols = (bw - 10) / 8;
    if (cols > EDIT_COLS) cols = EDIT_COLS;

    int start_x = bx + 5;
    int start_y = by + 5;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            char ch = edit_buf[r][c];
            if (ch == 0) ch = ' ';
            fb_draw_char(start_x + c * 8, start_y + r * 14, ch, C_TEXT, C_BG);
        }

    // Show cursor
    if (cur_row >= 0 && cur_row < rows && cur_col >= 0 && cur_col < cols)
        fb_fill_rect(start_x + cur_col * 8, start_y + cur_row * 14, 2, 8, C_CURS);

    // Status bar
    int sy = by + bh - 16;
    fb_fill_rect(bx + 1, sy, bw - 2, 15, C_STAT);
    char status[64];
    int n = 0;
    const char *fn = open_fname[0] ? open_fname : "(untitled)";
    while (*fn && n < 20) status[n++] = *fn++;
    status[n++] = ' ';
    status[n++] = '-';
    status[n++] = ' ';
    status[n++] = 'L';
    status[n++] = '0' + (cur_row + 1) / 10 % 10;
    status[n++] = '0' + (cur_row + 1) % 10;
    status[n++] = ':';
    status[n++] = '0' + (cur_col + 1) / 10 % 10;
    status[n++] = '0' + (cur_col + 1) % 10;
    if (dirty) { status[n++] = ' '; status[n++] = '*'; }
    status[n] = 0;
    fb_draw_string(bx + 5, sy + 3, status, C_TEXT, C_STAT);

    const char *hint = "ESC:Save&Close";
    int hl = 0; while (hint[hl]) hl++;
    fb_draw_string(bx + bw - hl * 8 - 6, sy + 3, hint, C_TEXT, C_STAT);
}

void notepad_run(void) {
    open_fname[0] = 0;
    edit_clear();

    int ww = 520, wh = 360;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    int win = gui_create_window(wx, wy, ww, wh, "Notepad");
    if (win < 0) return;
    gui_mark_app_window(win);
    gui_window_set_redraw_fn(win, notepad_redraw_fn);

    gui_redraw();

    int running = 1;
    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();

        if (c == 27) {
            if (dirty) edit_save();
            running = 0;
        } else if (c == '\n') {
            cur_row++;
            cur_col = 0;
            if (cur_row >= EDIT_ROWS) cur_row = EDIT_ROWS - 1;
            dirty = 1;
            gui_redraw();
        } else if (c == '\b') {
            if (cur_col > 0) {
                cur_col--;
                edit_buf[cur_row][cur_col] = ' ';
            } else if (cur_row > 0) {
                cur_row--;
                int c2 = EDIT_COLS - 2;
                while (c2 > 0 && edit_buf[cur_row][c2] == ' ') c2--;
                cur_col = c2 + 1;
                if (cur_col >= EDIT_COLS) cur_col = EDIT_COLS - 1;
            }
            dirty = 1;
            gui_redraw();
        } else if (c >= ' ') {
            edit_buf[cur_row][cur_col] = c;
            cur_col++;
            if (cur_col >= EDIT_COLS) { cur_col = 0; cur_row++; }
            if (cur_row >= EDIT_ROWS) cur_row = EDIT_ROWS - 1;
            dirty = 1;
            gui_redraw();
        }

        // Check for saved filename from shell
        // We handle F2 via a hack: check keyboard for F2 sequence
    }

    gui_redraw();
}

// Version that opens a specific file
void notepad_open(const char *fname) {
    int i;
    for (i = 0; fname[i] && i < 31; i++)
        open_fname[i] = fname[i];
    open_fname[i] = 0;
    if (fs_exists(fname))
        edit_load(fname);
    else {
        edit_clear();
        fs_create(fname);
    }
    notepad_run();
}
