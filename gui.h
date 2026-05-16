#ifndef GUI_H
#define GUI_H

typedef void (*window_draw_fn)(int id);

int gui_init(void);
void gui_redraw(void);
void gui_term_putchar(char c);
void gui_term_clear(void);
void gui_poll(void);
int gui_create_window(int x, int y, int w, int h, const char *title);
void gui_mark_app_window(int id);
void gui_window_set_redraw_fn(int id, window_draw_fn fn);
int gui_active_win(void);
void gui_get_body(int id, int *x, int *y, int *w, int *h);
void gui_cursor_hide(void);
void gui_cursor_show(void);
void gui_drag_start(const char *name);
const char *gui_drag_file(void);
void gui_set_window_title(int id, const char *title);
void gui_term_set_cursor_col(int col);
int gui_term_get_cursor_col(void);
void gui_term_write_cmd(const char *buf, int len);

#endif
