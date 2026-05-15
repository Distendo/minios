#include "settings_store.h"
#include "framebuffer.h"
#include "fs.h"

gui_settings_t gui_settings;

static gui_settings_t defaults = {
    .mouse_icon = 0,
    .bg_color = RGB(51, 102, 153),
    .bg_pattern = 0,
    .boot_anim = 1,
    .anim_speed = 2,
    .desktop_env = 0,
};

static int parse_int(const char *s, int len) {
    int v = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] >= '0' && s[i] <= '9')
            v = v * 10 + (s[i] - '0');
    }
    return v;
}

static int streq(const char *a, int alen, const char *b) {
    int i = 0;
    while (i < alen && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return b[i] == 0 && i == alen;
}

void settings_load(void) {
    gui_settings = defaults;
    uint8_t buf[512];
    int n = fs_read("/settings.cfg", buf, 512);
    if (n <= 0) return;
    int i = 0;
    while (i < n) {
        int j = i;
        while (j < n && buf[j] != '\n') j++;
        int eq = i;
        while (eq < j && buf[eq] != '=') eq++;
        if (eq > i && eq < j) {
            int klen = eq - i;
            int vlen = j - eq - 1;
            const char *key = (const char*)buf + i;
            const char *val = (const char*)buf + eq + 1;
            int v = parse_int(val, vlen);
            if (streq(key, klen, "mouse_icon")) gui_settings.mouse_icon = v;
            else if (streq(key, klen, "bg_color")) gui_settings.bg_color = v;
            else if (streq(key, klen, "bg_pattern")) gui_settings.bg_pattern = v;
            else if (streq(key, klen, "boot_anim")) gui_settings.boot_anim = v;
            else if (streq(key, klen, "anim_spd")) gui_settings.anim_speed = v;
            else if (streq(key, klen, "desktop_env")) gui_settings.desktop_env = v;
        }
        i = j + 1;
    }
}

static void s_puts(char **p, char *end, const char *s) {
    while (*s && *p < end) *(*p)++ = *s++;
}

static void s_putint(char **p, char *end, int v) {
    if (v == 0) { if (*p < end) *(*p)++ = '0'; return; }
    char r[16]; int ri = 0;
    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    while (v) { r[ri++] = '0' + (v % 10); v /= 10; }
    if (neg) r[ri++] = '-';
    while (ri && *p < end) *(*p)++ = r[--ri];
}

static void s_putu32(char **p, char *end, uint32_t v) {
    if (v == 0) { if (*p < end) *(*p)++ = '0'; return; }
    char r[16]; int ri = 0;
    while (v) { r[ri++] = '0' + (v % 10); v /= 10; }
    while (ri && *p < end) *(*p)++ = r[--ri];
}

void settings_save(void) {
    char buf[512];
    char *p = buf, *end = buf + 512;

    s_puts(&p, end, "mouse_icon="); s_putint(&p, end, gui_settings.mouse_icon); *p++ = '\n';
    s_puts(&p, end, "bg_color="); s_putu32(&p, end, gui_settings.bg_color); *p++ = '\n';
    s_puts(&p, end, "bg_pattern="); s_putint(&p, end, gui_settings.bg_pattern); *p++ = '\n';
    s_puts(&p, end, "boot_anim="); s_putint(&p, end, gui_settings.boot_anim); *p++ = '\n';
    s_puts(&p, end, "anim_spd="); s_putint(&p, end, gui_settings.anim_speed); *p++ = '\n';
    s_puts(&p, end, "desktop_env="); s_putint(&p, end, gui_settings.desktop_env); *p++ = '\n';

    if (!fs_exists("/settings.cfg")) fs_create("/settings.cfg");
    fs_write("/settings.cfg", (uint8_t*)buf, p - buf);
}
