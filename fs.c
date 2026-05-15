#include "fs.h"

typedef struct {
    char name[FS_NAME_LEN];
    uint32_t size;
    uint8_t *data;
    int used;
} fs_entry_t;

static fs_entry_t entries[FS_MAX_FILES];
static uint8_t data_pool[FS_POOL_SIZE];
static uint32_t pool_offset;
static int initialized;

extern int shell_run(const char *cmd);

static int strieq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void strcpy(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
}

static int strln(const char *s) {
    int n = 0;
    while (*s++) n++;
    return n;
}

static int endswith(const char *name, const char *ext) {
    int nl = strln(name), el = strln(ext);
    if (nl < el) return 0;
    name += nl - el;
    while (*name && *ext) {
        if (*name != *ext) return 0;
        name++; ext++;
    }
    return 1;
}

int fs_init(void) {
    if (initialized) return 1;
    for (int i = 0; i < FS_MAX_FILES; i++)
        entries[i].used = 0;
    pool_offset = 0;
    initialized = 1;
    return 1;
}

int fs_create(const char *name) {
    if (!initialized || !name || !*name) return 0;
    if (fs_exists(name)) return 0;
    int slot = -1;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!entries[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    strcpy(entries[slot].name, name);
    entries[slot].size = 0;
    entries[slot].data = 0;
    entries[slot].used = 1;
    return 1;
}

int fs_delete(const char *name) {
    if (!initialized || !name) return 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (entries[i].used && strieq(entries[i].name, name)) {
            entries[i].used = 0;
            entries[i].data = 0;
            entries[i].size = 0;
            return 1;
        }
    }
    return 0;
}

int fs_rename(const char *oldname, const char *newname) {
    if (!initialized || !oldname || !newname || !*newname) return 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (entries[i].used && strieq(entries[i].name, oldname)) {
            strcpy(entries[i].name, newname);
            return 1;
        }
    }
    return 0;
}

int fs_write(const char *name, const uint8_t *data, uint32_t size) {
    if (!initialized || !name || !data) return 0;
    if (size > FS_MAX_SIZE) return 0;
    int slot = -1;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (entries[i].used && strieq(entries[i].name, name)) { slot = i; break; }
    if (slot < 0) {
        for (int i = 0; i < FS_MAX_FILES; i++)
            if (!entries[i].used) { slot = i; break; }
        if (slot < 0) return 0;
        strcpy(entries[slot].name, name);
        entries[slot].used = 1;
        entries[slot].size = 0;
        entries[slot].data = 0;
    }
    if (pool_offset + size > FS_POOL_SIZE) return 0;
    entries[slot].data = &data_pool[pool_offset];
    for (uint32_t i = 0; i < size; i++)
        data_pool[pool_offset + i] = data[i];
    pool_offset += size;
    entries[slot].size = size;
    return 1;
}

int fs_read(const char *name, uint8_t *buf, uint32_t max) {
    if (!initialized || !name || !buf) return -1;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (entries[i].used && strieq(entries[i].name, name)) {
            uint32_t sz = entries[i].size;
            if (sz > max) sz = max;
            for (uint32_t j = 0; j < sz; j++)
                buf[j] = entries[i].data[j];
            return (int)sz;
        }
    }
    return -1;
}

uint32_t fs_size(const char *name) {
    if (!initialized || !name) return 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (entries[i].used && strieq(entries[i].name, name))
            return entries[i].size;
    return 0;
}

int fs_exists(const char *name) {
    if (!initialized || !name) return 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (entries[i].used && strieq(entries[i].name, name))
            return 1;
    return 0;
}

int fs_list(char names[][FS_NAME_LEN], int max) {
    if (!initialized) return 0;
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES && count < max; i++)
        if (entries[i].used)
            strcpy(names[count++], entries[i].name);
    return count;
}

int fs_run(const char *name) {
    if (!initialized || !name) return 0;
    uint32_t sz = fs_size(name);
    if (sz == 0 || sz >= 4096) return 0;
    uint8_t buf[4096];
    if (fs_read(name, buf, 4095) < 0) return 0;
    buf[sz] = 0;

    if (endswith(name, ".py")) {
        extern int py_run(const char *src);
        py_run((const char *)buf);
        return 1;
    }
    if (endswith(name, ".sh")) {
        char *p = (char *)buf;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\n' || *p == '\r') {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }
            char line[256];
            int li = 0;
            while (*p && *p != '\n' && li < 255)
                line[li++] = *p++;
            line[li] = 0;
            if (*p == '\n') p++;
            if (li > 0)
                shell_run(line);
        }
        return 1;
    }
    return 0;
}