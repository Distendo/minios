#include "apt.h"
#include "net.h"
#include "fs.h"
#include "vga.h"
#include <stdint.h>

#define APT_HOST "10.0.2.2"
#define APT_PORT 80
#define PKG_LIST "packages.list"

static void print_num(int v) {
    if (v < 0) { vga_putchar('-'); v = -v; }
    int d = 1, t = v;
    while (t >= 10) { d *= 10; t /= 10; }
    do { vga_putchar('0' + v / d); v %= d; d /= 10; } while (d);
}

static const char *skip_headers(const char *buf, int len, int *body_len) {
    int i = 0;
    while (i < len - 3) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            *body_len = len - (i + 4); return buf + i + 4;
        }
        if (buf[i] == '\n' && buf[i+1] == '\n') {
            *body_len = len - (i + 2); return buf + i + 2;
        }
        i++;
    }
    *body_len = 0; return 0;
}

static int find_pkg(const char *list, int list_len, const char *pkg_name, char *filename, int max_fn) {
    int i = 0;
    while (i < list_len) {
        if (list[i] == '\n' || list[i] == '\r') { i++; continue; }
        char name[64]; int ni = 0;
        while (i < list_len && list[i] != ' ' && list[i] != '\t' && list[i] != '\n' && ni < 63)
            name[ni++] = list[i++];
        name[ni] = 0;
        while (i < list_len && (list[i] == ' ' || list[i] == '\t')) i++;
        char fn[64]; int fi = 0;
        while (i < list_len && list[i] != '\n' && list[i] != '\r' && fi < 63)
            fn[fi++] = list[i++];
        fn[fi] = 0;
        while (i < list_len && list[i] != '\n') i++;
        if (i < list_len && list[i] == '\n') i++;
        if (i < list_len && list[i] == '\r') i++;

        int j = 0;
        while (name[j] && pkg_name[j] && name[j] == pkg_name[j]) j++;
        if (name[j] == 0 && pkg_name[j] == 0) {
            int k;
            for (k = 0; fn[k] && k < max_fn - 1; k++) filename[k] = fn[k];
            filename[k] = 0;
            return 1;
        }
    }
    return 0;
}

int apt_update(void) {
    char resp[4096];
    int len = net_http_get(APT_HOST, APT_PORT, "/packages/list.txt", resp, 4095);
    if (len <= 0) { vga_writestring("apt: fetch failed\n"); return 0; }
    resp[len] = 0;
    int body_len;
    const char *body = skip_headers(resp, len, &body_len);
    if (!body || body_len <= 0) { vga_writestring("apt: empty response\n"); return 0; }
    if (fs_write(PKG_LIST, (const uint8_t *)body, body_len)) {
        vga_writestring("apt: updated (");
        print_num(body_len);
        vga_writestring(" bytes)\n");
        return 1;
    }
    vga_writestring("apt: write failed\n");
    return 0;
}

int apt_list(void) {
    if (!fs_exists(PKG_LIST)) { vga_writestring("apt: run 'apt update' first\n"); return 0; }
    uint8_t buf[4096];
    int r = fs_read(PKG_LIST, buf, 4095);
    if (r <= 0) { vga_writestring("apt: empty list\n"); return 0; }
    buf[r] = 0;
    vga_writestring("Packages:\n");
    int i = 0;
    while (i < r) {
        if (buf[i] == '\n' || buf[i] == '\r') { i++; continue; }
        vga_putchar(' ');
        while (i < r && buf[i] != '\n' && buf[i] != '\r') vga_putchar(buf[i++]);
        vga_putchar('\n');
        if (i < r && buf[i] == '\n') i++;
        if (i < r && buf[i] == '\r') i++;
    }
    return 1;
}

int apt_install(const char *pkg) {
    if (!pkg || !*pkg) { vga_writestring("Usage: apt install <pkg>\n"); return 0; }
    if (!fs_exists(PKG_LIST)) { vga_writestring("apt: run 'apt update' first\n"); return 0; }
    uint8_t lb[4096];
    int r = fs_read(PKG_LIST, lb, 4095);
    if (r <= 0) { vga_writestring("apt: empty list\n"); return 0; }
    lb[r] = 0;
    char fn[64];
    if (!find_pkg((const char *)lb, r, pkg, fn, 64)) {
        vga_writestring("apt: pkg '"); vga_writestring(pkg); vga_writestring("' not found\n");
        return 0;
    }
    if (fs_exists(fn)) {
        vga_writestring("apt: '"); vga_writestring(fn); vga_writestring("' exists\n");
        return 0;
    }
    char path[128]; int pi = 0;
    const char *pp = "/packages/";
    while (*pp) path[pi++] = *pp++;
    int fi = 0;
    while (fn[fi] && pi < 127) path[pi++] = fn[fi++];
    path[pi] = 0;
    vga_writestring("apt: downloading '"); vga_writestring(fn); vga_writestring("'...\n");
    char resp[4096];
    int len = net_http_get(APT_HOST, APT_PORT, path, resp, 4095);
    if (len <= 0) { vga_writestring("apt: download failed\n"); return 0; }
    resp[len] = 0;
    int body_len;
    const char *body = skip_headers(resp, len, &body_len);
    if (!body || body_len <= 0) { vga_writestring("apt: empty response\n"); return 0; }
    if (fs_write(fn, (const uint8_t *)body, body_len)) {
        vga_writestring("apt: installed (");
        print_num(body_len);
        vga_writestring(" bytes)\n");
        return 1;
    }
    vga_writestring("apt: write failed\n");
    return 0;
}

int apt_remove(const char *pkg) {
    if (!pkg || !*pkg) { vga_writestring("Usage: apt remove <pkg>\n"); return 0; }
    if (fs_delete(pkg)) { vga_writestring("apt: removed\n"); return 1; }
    if (fs_exists(PKG_LIST)) {
        uint8_t lb[4096];
        int r = fs_read(PKG_LIST, lb, 4095);
        if (r > 0) {
            lb[r] = 0;
            char fn[64];
            if (find_pkg((const char *)lb, r, pkg, fn, 64) && fs_delete(fn)) {
                vga_writestring("apt: removed '"); vga_writestring(fn); vga_writestring("'\n");
                return 1;
            }
        }
    }
    vga_writestring("apt: not found\n");
    return 0;
}

int apt_download(const char *url, const char *outname) {
    if (!url || !*url || !outname || !*outname) {
        vga_writestring("Usage: apt download <url> <out>\n");
        return 0;
    }
    const char *u = url;
    if (u[0]=='h' && u[1]=='t' && u[2]=='t' && u[3]=='p' && u[4]==':' && u[5]=='/' && u[6]=='/') u += 7;
    char host[128]; int hp = 0;
    int port = 80;
    while (*u && *u != '/' && *u != ':' && hp < 127) host[hp++] = *u++;
    host[hp] = 0;
    if (*u == ':') {
        u++; port = 0;
        while (*u >= '0' && *u <= '9') { port = port * 10 + (*u - '0'); u++; }
    }
    char path[256]; int pp = 0;
    if (*u == '/') { while (*u && pp < 255) path[pp++] = *u++; }
    else { path[0] = '/'; path[1] = 0; }
    path[pp] = 0;
    if (fs_exists(outname)) { vga_writestring("apt: exists\n"); return 0; }
    vga_writestring("apt: downloading...\n");
    char resp[4096];
    int len = net_http_get(host, port, path, resp, 4095);
    if (len <= 0) { vga_writestring("apt: failed\n"); return 0; }
    resp[len] = 0;
    int body_len;
    const char *body = skip_headers(resp, len, &body_len);
    if (!body || body_len <= 0) { vga_writestring("apt: empty\n"); return 0; }
    if (fs_write(outname, (const uint8_t *)body, body_len)) {
        vga_writestring("apt: saved (");
        print_num(body_len);
        vga_writestring(" bytes)\n");
        return 1;
    }
    vga_writestring("apt: write failed\n");
    return 0;
}
