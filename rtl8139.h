#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

int rtl8139_init(void);
int rtl8139_send(const uint8_t *data, int len);
int rtl8139_recv(uint8_t *buf, int max);
void rtl8139_get_mac(uint8_t *mac);
int rtl8139_ok(void);
void rtl8139_dump_rx(void);

#endif