#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <stdint.h>

typedef struct {
    int mouse_icon;
    uint32_t bg_color;
    int bg_pattern;
    int boot_anim;
    int anim_speed;
    int desktop_env;
} gui_settings_t;

extern gui_settings_t gui_settings;

void settings_load(void);
void settings_save(void);

#endif
