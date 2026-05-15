#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_writechar(char c);
char serial_readchar(void);
int serial_has_data(void);

#endif
