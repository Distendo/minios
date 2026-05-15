#include "gles.h"
#include "framebuffer.h"

static int vp_x, vp_y, vp_w, vp_h;
static int cc_r, cc_g, cc_b;
static int cur_r, cur_g, cur_b;
static int mode;
static int drawing;
static int vert_count;
static int verts[4096][2];

void gles_init(void) {
    vp_x = 0; vp_y = 0; vp_w = fb.width; vp_h = fb.height;
    cc_r = 0; cc_g = 0; cc_b = 0;
    cur_r = 255; cur_g = 255; cur_b = 255;
    mode = GLES_POINTS;
    drawing = 0;
    vert_count = 0;
}

void gles_viewport(int x, int y, int w, int h) {
    vp_x = x; vp_y = y; vp_w = w; vp_h = h;
}

void gles_clear_color(int r, int g, int b) {
    cc_r = r; cc_g = g; cc_b = b;
}

void gles_color(int r, int g, int b) {
    cur_r = r; cur_g = g; cur_b = b;
}

void gles_clear(void) {
    fb_fill_rect(0, 0, fb.width, fb.height, RGB(cc_r, cc_g, cc_b));
}

static void putpixel(int x, int y) {
    int sx = vp_x + x;
    int sy = vp_y + y;
    if (sx >= 0 && sx < fb.width && sy >= 0 && sy < fb.height)
        fb.addr[sy * (fb.pitch / 4) + sx] = RGB(cur_r, cur_g, cur_b);
}

static void draw_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        putpixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2) {
    int pts[3][2] = {{x0, y0}, {x1, y1}, {x2, y2}};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2 - i; j++)
            if (pts[j][1] > pts[j + 1][1]) {
                int tx = pts[j][0], ty = pts[j][1];
                pts[j][0] = pts[j + 1][0]; pts[j][1] = pts[j + 1][1];
                pts[j + 1][0] = tx; pts[j + 1][1] = ty;
            }
    int y_start = pts[0][1];
    int y_end = pts[2][1];
    if (y_start < 0) y_start = 0;
    if (y_end >= fb.height) y_end = fb.height - 1;
    for (int y = y_start; y <= y_end; y++) {
        int xl = 999999, xr = -999999;
        for (int e = 0; e < 3; e++) {
            int ax = pts[e][0], ay = pts[e][1];
            int bx = pts[(e + 1) % 3][0], by = pts[(e + 1) % 3][1];
            if (ay == by) continue;
            if ((y < ay && y < by) || (y > ay && y > by)) continue;
            int x = ax + (y - ay) * (bx - ax) / (by - ay);
            if (x < xl) xl = x;
            if (x > xr) xr = x;
        }
        if (xl < 0) xl = 0;
        if (xr >= fb.width) xr = fb.width - 1;
        for (int x = xl; x <= xr; x++)
            putpixel(x, y);
    }
}

void gles_begin(int m) {
    mode = m;
    vert_count = 0;
    drawing = 1;
}

void gles_vertex2i(int x, int y) {
    if (!drawing) return;
    if (vert_count < 4096) {
        verts[vert_count][0] = x;
        verts[vert_count][1] = y;
        vert_count++;
    }
}

void gles_end(void) {
    if (!drawing) return;
    drawing = 0;
    if (mode == GLES_POINTS) {
        for (int i = 0; i < vert_count; i++)
            putpixel(verts[i][0], verts[i][1]);
    } else if (mode == GLES_LINES) {
        for (int i = 0; i < vert_count - 1; i += 2)
            draw_line(verts[i][0], verts[i][1], verts[i + 1][0], verts[i + 1][1]);
    } else if (mode == GLES_TRIANGLES) {
        for (int i = 0; i < vert_count - 2; i += 3)
            draw_triangle(verts[i][0], verts[i][1], verts[i + 1][0], verts[i + 1][1], verts[i + 2][0], verts[i + 2][1]);
    }
    vert_count = 0;
}
