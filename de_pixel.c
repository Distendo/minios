#include "de_pixel.h"
#include "gles.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"
#include "rtc.h"
#include <stdint.h>

typedef struct { float x, y, z; } vec3;

static void rot_x(vec3 *p, float a) {
    float rad = a * 3.14159f / 180.0f;
    float c = 1.0f - rad * rad / 2.0f + rad * rad * rad * rad / 24.0f;
    float s = rad - rad * rad * rad / 6.0f;
    float ny = p->y * c - p->z * s;
    float nz = p->y * s + p->z * c;
    p->y = ny; p->z = nz;
}

static void rot_y(vec3 *p, float a) {
    float rad = a * 3.14159f / 180.0f;
    float c = 1.0f - rad * rad / 2.0f + rad * rad * rad * rad / 24.0f;
    float s = rad - rad * rad * rad / 6.0f;
    float nx = p->x * c + p->z * s;
    float nz = -p->x * s + p->z * c;
    p->x = nx; p->z = nz;
}

static const vec3 cube_verts[8] = {
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
    {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}
};

static const int cube_edges[24] = {
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7
};

#define NUM_STARS 80
static int stars[NUM_STARS][2];

static void draw_gradient(void) {
    for (int y = 0; y < fb.height; y++) {
        int r = 8 + y * 48 / fb.height;
        int g = 16 + y * 36 / fb.height;
        int b = 40 + y * 30 / fb.height;
        uint32_t col = RGB(r, g, b);
        uint32_t *line = &fb.addr[y * (fb.pitch / 4)];
        for (int x = 0; x < fb.width; x++)
            line[x] = col;
    }
}

static void project_cube(float angle, int out[8][2]) {
    int cx = fb.width / 2, cy = fb.height / 2;
    float fov = 200.0f;
    for (int i = 0; i < 8; i++) {
        vec3 v = cube_verts[i];
        rot_x(&v, angle);
        rot_y(&v, angle * 0.7f);
        float scale = fov / (fov + v.z + 4.0f);
        out[i][0] = cx + (int)(v.x * scale * 120.0f);
        out[i][1] = cy + (int)(v.y * scale * 120.0f);
    }
}

void de_pixel_run(void) {
    gles_init();
    gles_viewport(0, 0, fb.width, fb.height);

    for (int i = 0; i < NUM_STARS; i++) {
        stars[i][0] = (i * 137 + 53) % fb.width;
        stars[i][1] = (i * 251 + 31) % (fb.height * 2 / 3);
    }

    float angle = 0;
    int running = 1;

    while (running) {
        keyboard_poll();
        char c = keyboard_getchar();
        if (c == 27) running = 0;

        mouse_poll();
        int mx, my;
        mouse_get_pos(&mx, &my);

        draw_gradient();

        gles_color(180, 200, 255);
        gles_begin(GLES_POINTS);
        for (int i = 0; i < NUM_STARS; i++)
            gles_vertex2i(stars[i][0], stars[i][1]);
        gles_end();

        int cube2d[8][2];
        project_cube(angle, cube2d);

        gles_color(60, 180, 255);
        gles_begin(GLES_LINES);
        for (int i = 0; i < 24; i += 2) {
            gles_vertex2i(cube2d[cube_edges[i]][0], cube2d[cube_edges[i]][1]);
            gles_vertex2i(cube2d[cube_edges[i + 1]][0], cube2d[cube_edges[i + 1]][1]);
        }
        gles_end();

        gles_color(100, 220, 255);
        gles_begin(GLES_TRIANGLES);
        for (int fi = 0; fi < 6; fi++) {
            static const int faces[6][4] = {
                {0,1,2,3}, {4,5,6,7}, {0,1,5,4},
                {2,3,7,6}, {0,3,7,4}, {1,2,6,5}
            };
            int ia = faces[fi][0], ib = faces[fi][1], ic = faces[fi][2];
            gles_vertex2i(cube2d[ia][0], cube2d[ia][1]);
            gles_vertex2i(cube2d[ib][0], cube2d[ib][1]);
            gles_vertex2i(cube2d[ic][0], cube2d[ic][1]);
            ia = faces[fi][0]; ib = faces[fi][2]; ic = faces[fi][3];
            gles_vertex2i(cube2d[ia][0], cube2d[ia][1]);
            gles_vertex2i(cube2d[ib][0], cube2d[ib][1]);
            gles_vertex2i(cube2d[ic][0], cube2d[ic][1]);
        }
        gles_end();

        rtc_time_t now;
        rtc_read_time(&now);
        char ts[12];
        ts[0] = '0' + now.hours / 10; ts[1] = '0' + now.hours % 10;
        ts[2] = ':';
        ts[3] = '0' + now.minutes / 10; ts[4] = '0' + now.minutes % 10;
        ts[5] = ':';
        ts[6] = '0' + now.seconds / 10; ts[7] = '0' + now.seconds % 10;
        ts[8] = 0;
        fb_draw_string(fb.width - 72, 12, ts, RGB(255, 255, 255), RGB(0, 0, 0));

        char info[] = "Pixel DE - ESC=exit";
        fb_draw_string(12, 12, info, RGB(180, 200, 220), RGB(0, 0, 0));

        fb_putpixel(mx, my, RGB(255, 255, 255));
        fb_putpixel(mx - 1, my, RGB(255, 255, 255));
        fb_putpixel(mx + 1, my, RGB(255, 255, 255));
        fb_putpixel(mx, my - 1, RGB(255, 255, 255));
        fb_putpixel(mx, my + 1, RGB(255, 255, 255));

        angle += 1.5f;
        if (angle >= 360.0f) angle -= 360.0f;

        for (volatile int d = 0; d < 80000; d++);
    }
}
