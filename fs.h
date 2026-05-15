#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAX_FILES 64
#define FS_NAME_LEN 32
#define FS_MAX_SIZE 4096
#define FS_POOL_SIZE 65536

int fs_init(void);
int fs_create(const char *name);
int fs_delete(const char *name);
int fs_rename(const char *oldname, const char *newname);
int fs_write(const char *name, const uint8_t *data, uint32_t size);
int fs_read(const char *name, uint8_t *buf, uint32_t max);
uint32_t fs_size(const char *name);
int fs_exists(const char *name);
int fs_list(char names[][FS_NAME_LEN], int max);
int fs_run(const char *name);

#endif