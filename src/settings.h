/*
 * settings.h - saved options (the original kept Size, Skill and Room in
 * entpack.ini [Rodent]); stored in $XDG_CONFIG_HOME/rodentrecomp/settings.ini
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

typedef struct {
    int  speed;           /* 0 Snail .. 4 Blazing */
    bool large;           /* 16x16 tiles */
    bool mono;            /* black and white */
    bool fullscreen;
    int  level;           /* starting level, 0-based */
    char pack[512];       /* file in DATA/packs, or a path */
    char skin[512];       /* folder in DATA/skins, or a path; "" = none */
} Settings;

void settings_defaults(Settings *s);
void settings_load(Settings *s);          /* missing file or keys keep the defaults */
bool settings_save(const Settings *s);
const char *settings_path(void);

#endif /* SETTINGS_H */
