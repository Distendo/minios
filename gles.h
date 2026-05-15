#ifndef GLES_H
#define GLES_H

#define GLES_POINTS 0
#define GLES_LINES 1
#define GLES_TRIANGLES 2

void gles_init(void);
void gles_viewport(int x, int y, int w, int h);
void gles_clear_color(int r, int g, int b);
void gles_color(int r, int g, int b);
void gles_clear(void);
void gles_begin(int mode);
void gles_vertex2i(int x, int y);
void gles_end(void);

#endif
