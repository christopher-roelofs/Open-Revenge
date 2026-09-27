/*
 * pack.c - level pack loader (format described in data/packs/README.md)
 */

#include "pack.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *f;
    const char *path;
    int line;
    char buf[512];
} Reader;

static bool fail(Reader *r, const char *msg, const char *arg)
{
    fprintf(stderr, "%s:%d: %s%s%s\n", r->path, r->line, msg, arg ? ": " : "", arg ? arg : "");
    return false;
}

/* next line with its ';' comment and surrounding whitespace removed */
static char *next_line(Reader *r)
{
    char *s, *e;
    if (!fgets(r->buf, sizeof r->buf, r->f)) return NULL;
    r->line++;
    if ((e = strchr(r->buf, ';'))) *e = '\0';
    for (s = r->buf; isspace((unsigned char)*s); s++) ;
    for (e = s + strlen(s); e > s && isspace((unsigned char)e[-1]); e--) ;
    *e = '\0';
    return s;
}

static bool parse_int(Reader *r, const char *key, const char *val, int lo, int hi, int *out)
{
    char *end;
    long v = strtol(val, &end, 10);
    if (*val == '\0' || *end != '\0' || v < lo || v > hi) {
        char msg[96];
        snprintf(msg, sizeof msg, "%s must be a number from %d to %d", key, lo, hi);
        return fail(r, msg, val);
    }
    *out = (int)v;
    return true;
}

static void copy_str(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src);
}

static bool read_grid(Reader *r, LevelDef *lv)
{
    int x, y;
    bool fMouse = false;

    lv->ccat = 0;
    for (y = 0; y < cFieldC; y++) {
        char *s;
        do {
            if (!(s = next_line(r)))
                return fail(r, "grid ends early: expected 23 rows", NULL);
        } while (*s == '\0');
        if ((int)strlen(s) != cFieldC)
            return fail(r, "grid rows must be 23 characters wide", s);
        for (x = 0; x < cFieldC; x++) {
            int ch;
            bool fEdge = x == 0 || y == 0 || x == cFieldCM1 || y == cFieldCM1;
            switch (s[x]) {
            case '.': ch = chOPEN;   break;
            case 'W': ch = chWALL;   break;
            case 'B': ch = chBLOCK;  break;
            case 'H': ch = chHOLE;   break;
            case 'T': ch = chTRAP;   break;
            case 'C': ch = chCHEESE; break;
            case 'M':
                if (fMouse) return fail(r, "more than one mouse (M) in grid", NULL);
                fMouse = true;
                lv->startX = x; lv->startY = y;
                ch = chOPEN;
                break;
            case 'K':
                if (lv->ccat == cbadMaxC) return fail(r, "too many cats (K) in grid, max 20", NULL);
                lv->catX[lv->ccat] = x; lv->catY[lv->ccat] = y; lv->ccat++;
                ch = chOPEN;
                break;
            default: {
                char c[2] = { s[x], 0 };
                return fail(r, "unknown grid character (use . W B H T C M K)", c);
            }
            }
            if (fEdge && ch != chWALL)
                return fail(r, "the outer ring of the grid must be walls (W)", s);
            lv->grid[y][x] = (uint8_t)ch;
        }
    }
    if (!fMouse) { lv->startX = cFieldCD2; lv->startY = cFieldCD2; }
    return true;
}

static bool finish_level(Reader *r, LevelDef *lv, bool fPattern)
{
    if (!fPattern && lv->pattern != patGRID)
        return fail(r, "level has neither a pattern nor a grid", lv->name);
    if (lv->goal == goalCHEESE) {
        int x, y, c = 0;
        if (lv->pattern == patGRID)
            for (y = 0; y < cFieldC; y++)
                for (x = 0; x < cFieldC; x++)
                    c += lv->grid[y][x] == chCHEESE;
        if (c == 0)
            return fail(r, "goal: cheese needs a grid with at least one cheese (C)", lv->name);
    }
    if (lv->goal == goalCATS && lv->batch == 0 && !(lv->pattern == patGRID && lv->ccat > 0))
        return fail(r, "goal: cats needs cats at the start: batch above 0 or a K in the grid", lv->name);
    return true;
}

bool pack_load(Pack *pack, const char *path)
{
    Reader r = { 0 };
    LevelDef *lv = NULL;
    bool fPattern = false, ok = true;
    char *s;

    memset(pack, 0, sizeof *pack);
    r.path = path;
    if (!(r.f = fopen(path, "r"))) {
        fprintf(stderr, "Cannot open level pack %s\n", path);
        return false;
    }
    while (ok && (s = next_line(&r))) {
        char *key, *val, *colon;
        if (*s == '\0') continue;
        if (strcmp(s, "[level]") == 0) {
            LevelDef *levels;
            if (lv && !(ok = finish_level(&r, lv, fPattern))) break;
            levels = realloc(pack->levels, (pack->clevel + 1) * sizeof *levels);
            if (!levels) { ok = fail(&r, "out of memory", NULL); break; }
            pack->levels = levels;
            lv = &pack->levels[pack->clevel++];
            memset(lv, 0, sizeof *lv);
            snprintf(lv->name, sizeof lv->name, "Level %d", pack->clevel);
            lv->pattern = -1;
            lv->blocks = 100;
            lv->batch = 3; lv->wave = 3; lv->interval = 5;
            lv->yarn = true;
            lv->goal = goalCLOCK;
            lv->startX = lv->startY = cFieldCD2;
            fPattern = false;
            continue;
        }
        if (!(colon = strchr(s, ':'))) { ok = fail(&r, "expected key: value", s); break; }
        *colon = '\0';
        key = s;
        for (val = colon + 1; isspace((unsigned char)*val); val++) ;
        for (s = colon; s > key && isspace((unsigned char)s[-1]); s--) ;
        *s = '\0';

        if (!lv) {                                  /* pack header */
            if      (!strcmp(key, "name"))        copy_str(pack->name, sizeof pack->name, val);
            else if (!strcmp(key, "author"))      copy_str(pack->author, sizeof pack->author, val);
            else if (!strcmp(key, "year"))        copy_str(pack->year, sizeof pack->year, val);
            else if (!strcmp(key, "description")) copy_str(pack->description, sizeof pack->description, val);
            else ok = fail(&r, "unknown pack key (before the first [level])", key);
            continue;
        }
        if (!strcmp(key, "name")) {
            copy_str(lv->name, sizeof lv->name, val);
        } else if (!strcmp(key, "pattern")) {
            if (lv->pattern == patGRID) { ok = fail(&r, "a level has either a pattern or a grid", NULL); break; }
            if      (!strcmp(val, "square"))        lv->pattern = patSQUARE;
            else if (!strcmp(val, "checker"))       lv->pattern = patCHECKER;
            else if (!strcmp(val, "scatter"))       lv->pattern = patSCATTER;
            else if (!strcmp(val, "checker-walls")) lv->pattern = patCHECKER_WALLS;
            else { ok = fail(&r, "pattern must be square, checker, scatter or checker-walls", val); break; }
            fPattern = true;
        } else if (!strcmp(key, "grid")) {
            if (fPattern) { ok = fail(&r, "a level has either a pattern or a grid", NULL); break; }
            if (*val) { ok = fail(&r, "grid rows start on the line after grid:", val); break; }
            lv->pattern = patGRID;
            ok = read_grid(&r, lv);
        } else if (!strcmp(key, "blocks")) {
            ok = parse_int(&r, key, val, 1, 100, &lv->blocks);
        } else if (!strcmp(key, "difficulty")) {
            ok = parse_int(&r, key, val, 0, 100, &lv->difficulty);
        } else if (!strcmp(key, "batch")) {
            ok = parse_int(&r, key, val, 0, 2 * cbadMaxC, &lv->batch);
        } else if (!strcmp(key, "wave")) {
            ok = parse_int(&r, key, val, 0, 2 * cbadMaxC, &lv->wave);
        } else if (!strcmp(key, "interval")) {
            ok = parse_int(&r, key, val, 1, 30, &lv->interval);
        } else if (!strcmp(key, "yarn")) {
            if      (!strcmp(val, "on"))  lv->yarn = true;
            else if (!strcmp(val, "off")) lv->yarn = false;
            else ok = fail(&r, "yarn must be on or off", val);
        } else if (!strcmp(key, "goal")) {
            if      (!strcmp(val, "clock"))  lv->goal = goalCLOCK;
            else if (!strcmp(val, "cheese")) lv->goal = goalCHEESE;
            else if (!strcmp(val, "cats"))   lv->goal = goalCATS;
            else ok = fail(&r, "goal must be clock, cheese or cats", val);
        } else {
            ok = fail(&r, "unknown level key", key);
        }
    }
    if (ok && lv) ok = finish_level(&r, lv, fPattern);
    if (ok && pack->clevel == 0) ok = fail(&r, "pack has no [level] sections", NULL);
    fclose(r.f);
    if (!ok) pack_free(pack);
    return ok;
}

void pack_free(Pack *pack)
{
    free(pack->levels);
    memset(pack, 0, sizeof *pack);
}
