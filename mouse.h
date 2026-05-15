#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
void mouse_handle_byte(uint8_t data);
void mouse_poll(void);
void mouse_get_pos(int *x, int *y);
int mouse_get_buttons(void);

#endif
