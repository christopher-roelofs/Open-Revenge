/*
 * menu.c - title, settings, pause and game-over menus (see menu.h)
 */

#include "menu.h"
#include "assets.h"
#include "game.h"
#include "platform.h"
#include "scores.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

enum {
    itemPLAY, itemPACK, itemLEVEL, itemSETTINGS, itemQUIT,
    itemSPEED, itemSIZE, itemDISPLAY, itemSKIN, itemFULLSCREEN, itemBACK,
    itemRESUME, itemTO_TITLE, itemAGAIN, itemSCORES, itemCLEAR
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

/* high scores */
static ScoreTable  table;                 /* shown by MENU_SCORES */
static int         hilite = -1;           /* the entry just added */
static MenuId      scores_parent = MENU_TITLE;
static bool        confirm_clear;
static char        name[NAME_MAX_ + 1];   /* MENU_NAME */
static int         cursor;
static long        new_score;
static int         new_level;
static const char  name_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.'";

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
    if (id == MENU_SCORES && menu != MENU_SCORES && menu != MENU_NAME) {
        scores_parent = menu;
        hilite = -1;
    }
    if (id == MENU_SCORES) scores_load(S->pack, &table);
    confirm_clear = false;
    menu = id;
    sel = 0;
}

void menu_game_over(int level_reached)
{
    scores_load(S->pack, &table);
    if (scores_rank(&table, g.score) < 0) {
        menu_show(MENU_GAMEOVER);
        return;
    }
    new_score = g.score;
    new_level = level_reached;
    snprintf(name, sizeof name, "%.*s", NAME_MAX_, S->name);
    cursor = (int)strlen(name);
    menu_show(MENU_NAME);
}

static void name_done(void)
{
    Score e = { .score = new_score, .level = new_level, .speed = S->speed };
    time_t now = time(NULL);
    int len = (int)strlen(name);

    while (len > 0 && name[len - 1] == ' ') name[--len] = '\0';
    if (len == 0) snprintf(name, sizeof name, "Mouse");
    snprintf(S->name, sizeof S->name, "%s", name);   /* EntPack.ini DefName */
    settings_save(S);
    snprintf(e.name, sizeof e.name, "%s", name);
    strftime(e.date, sizeof e.date, "%Y-%m-%d", localtime(&now));
    int r = scores_add(S->pack, &e);
    scores_parent = MENU_GAMEOVER;
    menu_show(MENU_SCORES);
    hilite = r;
}

void menu_text(int ch)
{
    int len = (int)strlen(name);
    if (menu != MENU_NAME || len >= NAME_MAX_ || ch < 32 || ch > 126) return;
    memmove(name + cursor + 1, name + cursor, (size_t)(len - cursor + 1));
    name[cursor++] = (char)ch;
}

/* name entry: type, or Up/Down through letters and Left/Right between them */
static void name_key(int vk)
{
    int len = (int)strlen(name);
    int step = 0;
    switch (vk) {
    case KEY_UP:    case KEY_NUM1 + 7: step = +1; break;
    case KEY_DOWN:  case KEY_NUM1 + 1: step = -1; break;
    case KEY_LEFT:  case KEY_NUM1 + 3: if (cursor > 0) cursor--; return;
    case KEY_RIGHT: case KEY_NUM1 + 5: if (cursor < len) cursor++; return;
    case KEY_BACK:
        if (cursor > 0) {
            memmove(name + cursor - 1, name + cursor, (size_t)(len - cursor + 1));
            cursor--;
        }
        return;
    case KEY_RETURN: case KEY_ESCAPE:
        name_done();
        return;
    default:
        return;
    }
    if (cursor == len) {                      /* past the end: start a new letter */
        if (len >= NAME_MAX_) return;
        name[len] = step > 0 ? 'A' : 'Z';
        name[len + 1] = '\0';
        return;
    }
    const char *p = strchr(name_chars, name[cursor]);
    int n = (int)sizeof name_chars - 1, i = p ? (int)(p - name_chars) : 0;
    name[cursor] = name_chars[((i + step) % n + n) % n];
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
        ITEM(itemSCORES, "High scores");
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
        ITEM(itemSCORES, "High scores");
        ITEM(itemTO_TITLE, "Title");
        break;
    case MENU_SCORES:
        ITEM(itemBACK, "Back");
        if (table.n) ITEM(itemCLEAR, confirm_clear ? "Really clear them?" : "Clear scores");
        break;
    case MENU_NAME:
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
    case itemBACK:
        if (menu == MENU_SCORES) {
            MenuId back = scores_parent;
            menu_show(back);
            sel = back == MENU_TITLE ? 3 : 1;     /* on "High scores" */
        } else {
            menu_show(parent);
            sel = parent == MENU_PAUSE ? 1 : 4;   /* on "Settings" */
        }
        break;
    case itemSCORES:   menu_show(MENU_SCORES); break;
    case itemCLEAR:                               /* the original's &Clear Scores */
        if (!confirm_clear) { confirm_clear = true; sel = 1; break; }
        scores_clear(S->pack);
        menu_show(MENU_SCORES);
        break;
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
    int n;
    if (menu == MENU_NAME) {
        name_key(vk);
        return true;
    }
    if ((n = items(it)) == 0) return true;
    sel = wrap(sel, n);
    switch (vk) {
    case KEY_UP:    case KEY_NUM1 + 7: sel = wrap(sel - 1, n); confirm_clear = false; break;
    case KEY_DOWN:  case KEY_NUM1 + 1: sel = wrap(sel + 1, n); confirm_clear = false; break;
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
        case MENU_SCORES:   return activate(itemBACK);
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

static void format_score(char *out, size_t n, long v)   /* Format$(v, "##,##0") */
{
    char digits[24];
    int len = snprintf(digits, sizeof digits, "%ld", v), o = 0, i;
    for (i = 0; i < len && o + 2 < (int)n; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = digits[i];
    }
    out[o] = '\0';
}

/* Height of what a menu shows between its title and its items; draws it
 * when draw is set, with (x, y, w) the space for it */
static int body(bool draw, int x, int y, int w)
{
    int hn = platform_text_height(FONT_NORMAL), h = 0, i;
    char buf[64];

    switch (menu) {
    case MENU_GAMEOVER:
        if (draw) {
            char sc[32];
            format_score(sc, sizeof sc, g.score);
            snprintf(buf, sizeof buf, "Score %s", sc);
            draw_centered(FONT_NORMAL, buf, x, w, y, 0x000000);
        }
        return hn + 6;

    case MENU_NAME:
        if (draw) {
            int cw = platform_text_width(FONT_BIG, "W"), bw = cw * NAME_MAX_ + 8;
            int bx = x + (w - bw) / 2, by = y + 2 * hn + 6, hb = platform_text_height(FONT_BIG);
            int len = (int)strlen(name), cx = bx + 4;
            draw_centered(FONT_NORMAL, "You have achieved a high score!", x, w, y, 0x000000);
            draw_centered(FONT_NORMAL, "Please enter your name:", x, w, y + hn, 0x000000);
            platform_fill_rect(bx - 1, by - 1, bw + 2, hb + 6, 0, 0, 0);
            platform_fill_rect(bx, by, bw, hb + 4, 255, 255, 255);
            for (i = 0; i <= len && i < NAME_MAX_; i++) {       /* one cell per letter */
                char c[2] = { i < len ? name[i] : ' ', 0 };
                uint32_t fg = 0x000000;
                if (i == cursor) {
                    platform_fill_rect(cx, by + 2, cw, hb, 0, 0, 0);
                    fg = 0xFFFFFF;
                }
                platform_draw_text(FONT_BIG, c, cx + (cw - platform_text_width(FONT_BIG, c)) / 2, by + 2, fg);
                cx += cw;
            }
            draw_centered(FONT_NORMAL, "Type, or \u2191\u2193 letter \u2190\u2192 move", x, w, by + hb + 10, 0x000000);
            draw_centered(FONT_NORMAL, "Enter / A: done", x, w, by + hb + 10 + hn, 0x000000);
        }
        return 2 * hn + 6 + platform_text_height(FONT_BIG) + 10 + 2 * hn + 6;

    case MENU_SCORES: {
        int idx = find(packs, npack, S->pack);
        const char *pname = idx >= 0 ? packs[idx].name : S->pack;
        h = hn + 4 + (table.n ? table.n : 1) * hn + 6;
        if (!draw) return h;
        draw_centered(FONT_NORMAL, fit(FONT_NORMAL, pname, w - 12), x, w, y, 0x000000);
        y += hn + 4;
        if (table.n == 0) {
            draw_centered(FONT_NORMAL, "No scores yet", x, w, y, 0x000000);
            return h;
        }
        int rx = x + w - 6, lvw = platform_text_width(FONT_NORMAL, "L50");
        for (i = 0; i < table.n; i++, y += hn) {
            const Score *e = &table.s[i];
            uint32_t fg = 0x000000;
            char sc[32], lv[8];
            if (i == hilite) {                    /* the score just entered */
                platform_fill_rect(x + 2, y, w - 4, hn, 0, 0, 0);
                fg = 0xFFFFFF;
            }
            snprintf(buf, sizeof buf, "%d.", i + 1);
            platform_draw_text(FONT_NORMAL, buf, x + 6 + platform_text_width(FONT_NORMAL, "10.")
                               - platform_text_width(FONT_NORMAL, buf), y, fg);
            platform_draw_text(FONT_NORMAL, e->name, x + 12 + platform_text_width(FONT_NORMAL, "10."), y, fg);
            snprintf(lv, sizeof lv, "L%d", e->level);
            platform_draw_text(FONT_NORMAL, lv, rx - lvw, y, fg);
            format_score(sc, sizeof sc, e->score);
            platform_draw_text(FONT_NORMAL, sc, rx - lvw - 8 - platform_text_width(FONT_NORMAL, sc), y, fg);
        }
        return h;
    }
    default:
        return 0;
    }
}

void menu_paint(void)
{
    Item it[8];
    int n = items(it), fx, fy, fw, fh, i;
    int hb = platform_text_height(FONT_BIG), hn = platform_text_height(FONT_NORMAL);
    int tw = platform_tile_width(), row = hn + 4, pad = 6;
    const char *title;

    switch (menu) {
    case MENU_TITLE:    title = "Rodent's Revenge"; break;
    case MENU_SETTINGS: title = "Settings"; break;
    case MENU_PAUSE:    title = "Paused"; break;
    case MENU_GAMEOVER: title = "Game Over"; break;
    case MENU_NAME:     title = "High Score"; break;
    case MENU_SCORES:   title = "Hall of Fame"; break;   /* WEPFAME's caption */
    default:            return;
    }

    platform_field_rect(&fx, &fy, &fw, &fh);
    int w = fw - 4 * tw;
    int hbody = body(false, 0, 0, w);
    int h = pad + hb + pad + hbody + n * row + pad;
    int x = fx + (fw - w) / 2, y = fy + (fh - h) / 2;

    platform_fill_rect(x - 1, y - 1, w + 2, h + 2, 0, 0, 0);
    platform_fill_rect(x, y, w, h, 255, 255, 255);
    y += pad;
    draw_centered(FONT_BIG, title, x, w, y, 0x000000);
    y += hb + pad;
    body(true, x, y, w);
    y += hbody;

    if (n) sel = wrap(sel, n);
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
            snprintf(v, sizeof v, "\u25C2 %s \u25B8",
                     fit(FONT_NORMAL, it[i].value, rx - lx - lw - 12 - platform_text_width(FONT_NORMAL, "\u25C2  \u25B8")));
            platform_draw_text(FONT_NORMAL, v, rx - platform_text_width(FONT_NORMAL, v), y + 2, fg);
        } else {
            draw_centered(FONT_NORMAL, it[i].label, x, w, y + 2, fg);
        }
    }
}
