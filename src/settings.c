/*
 * settings.c - settings.ini: "key=value" lines (see settings.h)
 */

#include "settings.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *yes_no(bool b) { return b ? "yes" : "no"; }

void settings_defaults(Settings *s)
{
    memset(s, 0, sizeof *s);
    s->speed = 1;                             /* GetPrivateProfileInt("Skill", 1) */
    snprintf(s->pack, sizeof s->pack, "original.pack");
}

static const char *config_dir(void)
{
    static char dir[1024];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)        snprintf(dir, sizeof dir, "%s/rodentrecomp", xdg);
    else if (home && *home) snprintf(dir, sizeof dir, "%s/.config/rodentrecomp", home);
    else                    snprintf(dir, sizeof dir, ".");
    return dir;
}

const char *settings_path(void)
{
    static char path[1100];
    snprintf(path, sizeof path, "%s/settings.ini", config_dir());
    return path;
}

void settings_load(Settings *s)
{
    char line[1100];
    FILE *f = fopen(settings_path(), "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *val, *end;
        if (!eq || line[0] == ';' || line[0] == '#') continue;
        *eq = '\0';
        val = eq + 1;
        end = val + strcspn(val, "\r\n");
        *end = '\0';
        if      (!strcmp(line, "speed"))      { int v = atoi(val); if (v >= 0 && v <= 4) s->speed = v; }
        else if (!strcmp(line, "large"))      s->large = !strcmp(val, "yes");
        else if (!strcmp(line, "mono"))       s->mono = !strcmp(val, "yes");
        else if (!strcmp(line, "fullscreen")) s->fullscreen = !strcmp(val, "yes");
        else if (!strcmp(line, "level"))      { int v = atoi(val); if (v >= 1 && v <= 50) s->level = v - 1; }
        else if (!strcmp(line, "pack") && *val) snprintf(s->pack, sizeof s->pack, "%s", val);
        else if (!strcmp(line, "skin"))       snprintf(s->skin, sizeof s->skin, "%s", val);
    }
    fclose(f);
}

static bool make_dirs(const char *dir)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", dir);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(buf, 0755) == 0 || errno == EEXIST;
}

bool settings_save(const Settings *s)
{
    FILE *f;
    if (!make_dirs(config_dir()) || !(f = fopen(settings_path(), "w"))) {
        fprintf(stderr, "Cannot save %s\n", settings_path());
        return false;
    }
    fprintf(f, "; Rodent's Revenge settings (written by the game)\n");
    fprintf(f, "speed=%d\nlarge=%s\nmono=%s\nfullscreen=%s\nlevel=%d\npack=%s\nskin=%s\n",
            s->speed, yes_no(s->large), yes_no(s->mono), yes_no(s->fullscreen),
            s->level + 1, s->pack, s->skin);
    fclose(f);
    return true;
}
