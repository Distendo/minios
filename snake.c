#include "snake.h"
#include "gui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "ports.h"

#define GW 30
#define GH 25
#define CELL 8

static int grid[GW][GH];
static int sx[GW * GH], sy[GW * GH];
static int sn;
static int fx, fy;
static int dir;
static int score;
static int game_over;
static int last_score;
static int snake_win;
static int s_bx, s_by, s_bw, s_bh;

static void spawn_food(void) {
    int n = 0;
    int candidates[GW * GH][2];
    for (int x = 1; x < GW - 1; x++)
        for (int y = 1; y < GH - 1; y++)
            if (!grid[x][y]) { candidates[n][0] = x; candidates[n][1] = y; n++; }
    if (n == 0) return;
    int i = 0;
    for (volatile int c = 0; c < 100; c++) i = (i + 1) % n;
    fx = candidates[i][0];
    fy = candidates[i][1];
}

static void reset_game(void) {
    for (int x = 0; x < GW; x++)
        for (int y = 0; y < GH; y++)
            grid[x][y] = 0;
    for (int x = 0; x < GW; x++) { grid[x][0] = 1; grid[x][GH - 1] = 1; }
    for (int y = 0; y < GH; y++) { grid[0][y] = 1; grid[GW - 1][y] = 1; }
    sn = 3;
    sx[0] = 5; sy[0] = 5;
    sx[1] = 4; sy[1] = 5;
    sx[2] = 3; sy[2] = 5;
    for (int i = 0; i < sn; i++) grid[sx[i]][sy[i]] = 1;
    dir = 1;
    score = 0;
    last_score = 0;
    game_over = 0;
    spawn_food();
}

static int tick;

static void game_tick(void) {
    if (game_over) return;
    int nx = sx[0], ny = sy[0];
    if (dir == 0) ny--;
    else if (dir == 1) nx++;
    else if (dir == 2) ny++;
    else nx--;
    if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) { game_over = 1; return; }
    int ate = (nx == fx && ny == fy);
    if (grid[nx][ny] && !(ate && nx == sx[sn - 1] && ny == sy[sn - 1])) { game_over = 1; return; }
    for (int i = sn - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = nx; sy[0] = ny;
    if (!ate) { grid[sx[sn - 1]][sy[sn - 1]] = 0; }
    else { sn++; sx[sn - 1] = sx[sn - 2]; sy[sn - 1] = sy[sn - 2]; score++; spawn_food(); }
    if (score != last_score) {
        last_score = score;
        char title[32];
        int ti = 0;
        const char *base = "Snake  Score:";
        while (*base && ti < 24) title[ti++] = *base++;
        if (score >= 100) title[ti++] = '0' + score / 100;
        if (score >= 10) title[ti++] = '0' + (score / 10) % 10;
        title[ti++] = '0' + score % 10;
        title[ti] = 0;
        gui_set_window_title(snake_win, title);
    }
    grid[nx][ny] = 1;
}

static void draw_game(int bx, int by) {
    for (int x = 0; x < GW; x++)
        for (int y = 0; y < GH; y++) {
            int px = bx + x * CELL;
            int py = by + y * CELL;
            uint32_t col;
            if (x == 0 || x == GW - 1 || y == 0 || y == GH - 1)
                col = RGB(100, 100, 100);
            else if (x == fx && y == fy)
                col = RGB(255, 0, 0);
            else if (grid[x][y])
                col = RGB(0, 180, 0);
            else
                col = RGB(30, 30, 30);
            fb_fill_rect(px, py, CELL - 1, CELL - 1, col);
        }
    if (game_over)
        fb_draw_string(bx + 40, by + GH * CELL / 2 - 8,
                       "GAME OVER - press R to restart", RGB(255, 0, 0), RGB(30, 30, 30));
}

static void snake_redraw_fn(int id) {
    int bx, by, bw, bh;
    gui_get_body(id, &bx, &by, &bw, &bh);
    bx += 1; by += 1;
    fb_fill_rect(bx, by, bw - 2, bh - 2, RGB(30, 30, 30));
    draw_game(bx, by);
}

void snake_run(void) {
    int ww = GW * CELL + 2;
    int wh = GH * CELL + 2;
    int wx = (800 - ww) / 2;
    int wy = (600 - 22 - wh) / 2;
    snake_win = gui_create_window(wx, wy, ww, wh + 18, "Snake");
    if (snake_win < 0) return;
    gui_mark_app_window(snake_win);
    gui_window_set_redraw_fn(snake_win, snake_redraw_fn);

    reset_game();
    gui_redraw();

    int bx, by, bw, bh;
    gui_get_body(snake_win, &bx, &by, &bw, &bh);
    s_bx = bx + 1; s_by = by + 1;
    s_bw = bw - 2; s_bh = bh - 2;

    tick = 0;
    int running = 1;
    int dir_pending = -1;

    while (running) {
        gui_poll();
        keyboard_poll();
        char c = keyboard_getchar();
        if (c == 27) running = 0;
        if (c == 'r' || c == 'R') { reset_game(); tick = 0; }
        if (c == 'a' || c == 'A' || c == 4) dir_pending = 3;
        if (c == 'd' || c == 'D' || c == 6) dir_pending = 1;
        if (c == 'w' || c == 'W' || c == 23) dir_pending = 0;
        if (c == 's' || c == 'S' || c == 19) dir_pending = 2;

        tick++;
        if (tick % 6 == 0) {
            if (dir_pending >= 0) {
                if ((dir == 0 && dir_pending != 2) ||
                    (dir == 2 && dir_pending != 0) ||
                    (dir == 1 && dir_pending != 3) ||
                    (dir == 3 && dir_pending != 1))
                    dir = dir_pending;
                dir_pending = -1;
            }
            game_tick();
            gui_cursor_hide();
            fb_fill_rect(s_bx, s_by, s_bw, s_bh, RGB(30, 30, 30));
            draw_game(s_bx, s_by);
            gui_cursor_show();
        }
    }

    gui_redraw();
}