#ifndef NET_H
#define NET_H

#include <stdint.h>

int net_init(void);
int net_http_get(const char *host, int port, const char *path, char *resp, int max_resp);

#endif