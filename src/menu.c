/*
 * menu.c - title, settings, pause and game-over menus (see menu.h)
 */

#include "menu.h"
#include "assets.h"
#include "game.h"
#include "platform.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    itemPLAY, itemPACK, itemLEVEL, itemSETTINGS, itemQUIT,
    itemSPEED, itemSIZE, itemDISPLAY, itemSKIN, itemFULLSCREEN, itemBACK,
    itemRESUME, itemTO_TITLE, itemAGAIN
};

typedef struct { int id; const char *label; char value[80]; } Item;

typedef struct {
    char id[512];       /* what Settings stores: a name in data_dir, or a path */
    char name[64];      /* shown in the menu */
} Entry;

static const char *speed_names[5] = { "Snail", "Slow", "Medium", "Fast", "Blazing" };

static Settings   *S;
static Pack       *P;
static const char *data_dir;
static Entry      *packs, *skins;
static int         npack, nskin;
static MenuId      menu = MENU_NONE, parent = MENU_TITLE;
static int         sel;

/* ---- packs and skins ---- */

static void pack_path(const char *id, char *out, size_t n)
{
    if (strchr(id, '/')) snprintf(out, n, "%s", id);
    else                 snprintf(out, n, "%s/packs/%s", data_dir, id);
}

static const char *skin_path(const char *id)
{
    static char path[1100];
    if (!*id) return NULL;
    if (strchr(id, '/')) snprintf(path, sizeof path, "%s", id);
    else                 snprintf(path, sizeof path, "%s/skins/%s", data_dir, id);
    return path;
}

static Entry *add(Entry **list, int *n, const char *id, const char *name)
{
    Entry *e = realloc(*list, (*n + 1) * sizeof **list);
    if (!e) return NULL;
    *list = e;
    e = &e[(*n)++];
    snprintf(e->id, sizeof e->id, "%s", id);
    snprintf(e->name, sizeof e->name, "%s", name);
    return e;
}

static int find(const Entry *list, int n, const char *id)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(list[i].id, id)) return i;
    return -1;
}

static int by_name(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    /* the original game's pack first */
    if (!strcmp(x->id, "original.pack")) return -1;
    if (!strcmp(y->id, "original.pack")) return 1;
    return strcmp(x->name, y->name);
}

static void add_pack(const char *id)
{
    char path[1100];
    Pack p;
    pack_path(id, path, sizeof path);
    if (!pack_load(&p, path)) return;             /* the loader says why */
    add(&packs, &npack, id, p.name[0] ? p.name : id);
    pack_free(&p);
}

static void scan(void)
{
    char dir[1100];
    struct dirent *de;
    DIR *d;

    snprintf(dir, sizeof dir, "%s/packs", data_dir);
    if ((d = opendir(dir))) {
        while ((de = readdir(d))) {
            size_t n = strlen(de->d_name);
            if (n > 5 && !strcmp(de->d_name + n - 5, ".pack")) add_pack(de->d_name);
        }
        closedir(d);
    }
    if (npack) qsort(packs, npack, sizeof *packs, by_name);
    if (find(packs, npack, S->pack) < 0) add_pack(S->pack);      /* --pack FILE */

    add(&skins, &nskin, "", "None");
    snprintf(dir, sizeof dir, "%s/skins", data_dir);
    if ((d = opendir(dir))) {
        while ((de = readdir(d))) {
            char path[1400];
            struct stat st;
            if (de->d_name[0] == '.') continue;
            snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) add(&skins, &nskin, de->d_name, de->d_name);
        }
        closedir(d);
    }
    if (nskin > 1) qsort(skins + 1, nskin - 1, sizeof *skins, by_name);
    if (find(skins, nskin, S->skin) < 0) {                        /* --skin DIR */
        const char *slash = strrchr(S->skin, '/');
        add(&skins, &nskin, S->skin, slash && slash[1] ? slash + 1 : S->skin);
    }
}

static bool load_pack(const char *id_)
{
    char id[sizeof S->pack], path[1100];
    Pack p;
    snprintf(id, sizeof id, "%s", id_);       /* id_ may be S->pack itself */
    pack_path(id, path, sizeof path);
    if (!pack_load(&p, path)) return false;
    pack_free(P);
    *P = p;
    snprintf(S->pack, sizeof S->pack, "%s", id);
    return true;
}

bool menu_init(Settings *s, Pack *pack, const char *dir)
{
    S = s;
    P = pack;
    data_dir = dir;
    memset(P, 0, sizeof *P);
    scan();
    if (!load_pack(S->pack)) {
        fprintf(stderr, "Using the original levels instead of %s\n", S->pack);
        if (!load_pack("original.pack")) return false;
    }
    assets_set_skin(skin_path(S->skin));
    return true;
}

void menu_free(void)
{
    free(packs); free(skins);
    packs = skins = NULL;
    npack = nskin = 0;
}

/* ---- menus ---- */

MenuId menu_current(void) { return menu; }

void menu_show(MenuId id)
{
    if (id == MENU_SETTINGS && menu != MENU_SETTINGS) parent = menu;
    menu = id;
    sel = 0;
}

static int items(Item *it)
{
    int n = 0, i;
#define ITEM(i_, l_) (it[n].id = (i_), it[n].label = (l_), it[n].value[0] = '\0', &it[n++])
    switch (menu) {
    case MENU_TITLE:
        ITEM(itemPLAY, "Play");
        i = find(packs, npack, S->pack);
        snprintf(ITEM(itemPACK, "Levels")->value, sizeof it->value, "%.79s", i >= 0 ? packs[i].name : S->pack);
        snprintf(ITEM(itemLEVEL, "Start at level")->value, sizeof it->value, "%d", S->level + 1);
        ITEM(itemSETTINGS, "Settings");
        ITEM(itemQUIT, "Quit");
        break;
    case MENU_SETTINGS:
        snprintf(ITEM(itemSPEED, "Speed")->value, sizeof it->value, "%s", speed_names[S->speed]);
        snprintf(ITEM(itemSIZE, "Board size")->value, sizeof it->value, "%s", S->large ? "Large" : "Small");
        snprintf(ITEM(itemDISPLAY, "Display")->value, sizeof it->value, "%s", S->mono ? "Mono" : "Colour");
        i = find(skins, nskin, S->skin);
        snprintf(ITEM(itemSKIN, "Skin")->value, sizeof it->value, "%.79s", i >= 0 ? skins[i].name : S->skin);
        snprintf(ITEM(itemFULLSCREEN, "Fullscreen")->value, sizeof it->value, "%s", S->fullscreen ? "On" : "Off");
        ITEM(itemBACK, "Back");
        break;
    case MENU_PAUSE:
        ITEM(itemRESUME, "Resume");
        ITEM(itemSETTINGS, "Settings");
        ITEM(itemTO_TITLE, "Quit to title");
        break;
    case MENU_GAMEOVER:
        ITEM(itemAGAIN, "Play again");
        ITEM(itemTO_TITLE, "Title");
        break;
    case MENU_NONE:
        break;
    }
#undef ITEM
    return n;
}

static int wrap(int v, int n) { return ((v % n) + n) % n; }

/* ◂ ▸ on a value item: change it and apply at once */
static void change(int id, int d)
{
    int i;
    switch (id) {
    case itemPACK:
        if (npack < 2) return;
        i = find(packs, npack, S->pack);
        for (int tries = 0; tries < npack; tries++) {
            i = wrap(i + d, npack);
            if (load_pack(packs[i].id)) break;
        }
        game_init(S->speed, S->level, P, 0);
        set_speed(S->speed);
        break;
    case itemLEVEL:
        S->level = wrap(S->level + d, 50);
        game_set_start_level(S->level);
        break;
    case itemSPEED:
        S->speed = wrap(S->speed + d, 5);
        set_speed(S->speed);
        break;
    case itemSIZE:
        S->large = !S->large;
        platform_configure(S->large, S->mono);
        break;
    case itemDISPLAY:
        S->mono = !S->mono;
        platform_configure(S->large, S->mono);
        g.fMono = S->mono;
        break;
    case itemSKIN:
        i = wrap(find(skins, nskin, S->skin) + d, nskin);
        snprintf(S->skin, sizeof S->skin, "%s", skins[i].id);
        assets_set_skin(skin_path(S->skin));
        platform_load_graphics();
        break;
    case itemFULLSCREEN:
        S->fullscreen = !S->fullscreen;
        platform_set_fullscreen(S->fullscreen);
        break;
    default:
        return;
    }
    settings_save(S);
}

static bool activate(int id)
{
    switch (id) {
    case itemPLAY: case itemAGAIN:
        menu = MENU_NONE;
        new_game();
        break;
    case itemSETTINGS: menu_show(MENU_SETTINGS); break;
    case itemQUIT:     return false;
    case itemBACK:     menu_show(parent); sel = parent == MENU_PAUSE ? 1 : 3; break;
    case itemRESUME:   menu = MENU_NONE; break;
    case itemTO_TITLE:
        if (g.mode != modeDEMO) mode_set(modeDEMO);
        menu_show(MENU_TITLE);
        break;
    default:
        change(id, +1);                           /* Enter on a value steps it */
        break;
    }
    return true;
}

bool menu_key(int vk)
{
    Item it[8];
    int n = items(it);
    if (n == 0) return true;
    sel = wrap(sel, n);
    switch (vk) {
    case KEY_UP:    case KEY_NUM1 + 7: sel = wrap(sel - 1, n); break;
    case KEY_DOWN:  case KEY_NUM1 + 1: sel = wrap(sel + 1, n); break;
    case KEY_LEFT:  case KEY_NUM1 + 3: change(it[sel].id, -1); break;
    case KEY_RIGHT: case KEY_NUM1 + 5: change(it[sel].id, +1); break;
    case KEY_RETURN: case KEY_SPACE:   return activate(it[sel].id);
    case KEY_F2:
        if (menu == MENU_TITLE || menu == MENU_GAMEOVER) return activate(itemPLAY);
        break;
    case KEY_ESCAPE: case KEY_BACK:
        switch (menu) {
        case MENU_SETTINGS: return activate(itemBACK);
        case MENU_PAUSE:    return activate(itemRESUME);
        case MENU_GAMEOVER: return activate(itemTO_TITLE);
        default:            break;
        }
        break;
    }
    return true;
}

/* text cut to maxw with "..." */
static const char *fit(int font, const char *s, int maxw)
{
    static char buf[96];
    size_t n = strlen(s);
    snprintf(buf, sizeof buf, "%s", s);
    while (n > 0 && platform_text_width(font, buf) > maxw) {
        n--;
        snprintf(buf, sizeof buf, "%.*s...", (int)n, s);
    }
    return buf;
}

static void draw_centered(int font, const char *s, int x, int w, int y, uint32_t rgb)
{
    platform_draw_text(font, s, x + (w - platform_text_width(font, s)) / 2, y, rgb);
}

void menu_paint(void)
{
    Item it[8];
    int n = items(it), fx, fy, fw, fh, i;
    int hb = platform_text_height(FONT_BIG), hn = platform_text_height(FONT_NORMAL);
    int tw = platform_tile_width(), row = hn + 4, pad = 6;
    char score[48] = "";
    if (n == 0) return;

    if (menu == MENU_GAMEOVER) {                  /* Format$(scoreG, "##,##0") */
        char digits[24];
        int len = snprintf(digits, sizeof digits, "%ld", g.score), o = 0;
        o = snprintf(score, sizeof score, "Score ");
        for (i = 0; i < len; i++) {
            if (i > 0 && (len - i) % 3 == 0) score[o++] = ',';
            score[o++] = digits[i];
        }
        score[o] = '\0';
    }

    platform_field_rect(&fx, &fy, &fw, &fh);
    int w = fw - 4 * tw;
    int h = pad + hb + pad + (score[0] ? hn + pad : 0) + n * row + pad;
    int x = fx + (fw - w) / 2, y = fy + (fh - h) / 2;

    platform_fill_rect(x - 1, y - 1, w + 2, h + 2, 0, 0, 0);
    platform_fill_rect(x, y, w, h, 255, 255, 255);
    y += pad;
    draw_centered(FONT_BIG,
        menu == MENU_TITLE ? "Rodent's Revenge" : menu == MENU_SETTINGS ? "Settings" :
        menu == MENU_PAUSE ? "Paused" : "Game Over", x, w, y, 0x000000);
    y += hb + pad;
    if (score[0]) {
        draw_centered(FONT_NORMAL, score, x, w, y, 0x000000);
        y += hn + pad;
    }
    sel = wrap(sel, n);
    for (i = 0; i < n; i++, y += row) {
        uint32_t fg = 0x000000;
        int lx = x + pad, rx = x + w - pad;
        if (i == sel) {                           /* inverted bar, readable in mono too */
            platform_fill_rect(x + 2, y, w - 4, row, 0, 0, 0);
            fg = 0xFFFFFF;
        }
        if (it[i].value[0]) {
            char v[128];
            int lw = platform_text_width(FONT_NORMAL, it[i].label);
            platform_draw_text(FONT_NORMAL, it[i].label, lx, y + 2, fg);
            snprintf(v, sizeof v, "◂ %s ▸",
                     fit(FONT_NORMAL, it[i].value, rx - lx - lw - 12 - platform_text_width(FONT_NORMAL, "◂  ▸")));
            platform_draw_text(FONT_NORMAL, v, rx - platform_text_width(FONT_NORMAL, v), y + 2, fg);
        } else {
            draw_centered(FONT_NORMAL, it[i].label, x, w, y + 2, fg);
        }
    }
}
