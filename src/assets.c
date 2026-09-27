/*
 * assets.c - game graphics from rodent.exe (see assets.h)
 */

#include "assets.h"
#include <SDL.h>
#include <SDL_image.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../third_party/stb_image_write.h"   /* implementation is in platform.c */

#define MAX_EXE_SIZE (4 * 1024 * 1024)

static const struct { const char *name; int w, h, nth; } g_spec[ASSET_COUNT] = {
    [ASSET_SPRITES_SMALL_COLOR] = { "sprites_small_color", 108, 36, 0 },
    [ASSET_SPRITES_SMALL_MONO]  = { "sprites_small_mono",  108, 36, 1 },
    [ASSET_SPRITES_LARGE_COLOR] = { "sprites_large_color", 144, 48, 0 },
    [ASSET_SPRITES_LARGE_MONO]  = { "sprites_large_mono",  144, 48, 1 },
    [ASSET_TIMER]               = { "timer",                32, 32, 0 },
};

static uint8_t *g_exe;
static size_t   g_exe_size;
static char     g_exe_path[1024];
static char     g_skin[1024];
static struct { size_t off, size; } g_bmp[ASSET_COUNT];

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static bool read_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) <= 0 || n > MAX_EXE_SIZE) { fclose(f); return false; }
    rewind(f);
    *data = malloc((size_t)n);
    if (!*data || fread(*data, 1, (size_t)n, f) != (size_t)n) { free(*data); *data = NULL; fclose(f); return false; }
    fclose(f);
    *size = (size_t)n;
    return true;
}

/* Every complete 4bpp "BM" file inside the exe, matched to g_spec by size
 * and order (the colour sheet comes before the mono one). */
static bool find_bitmaps(void)
{
    int seen[ASSET_COUNT] = { 0 };
    size_t off;
    int id;

    memset(g_bmp, 0, sizeof g_bmp);
    for (off = 0; off + 54 <= g_exe_size; off++) {
        const uint8_t *p = g_exe + off;
        uint32_t size;
        int w, h;
        if (p[0] != 'B' || p[1] != 'M') continue;
        size = rd32(p + 2);
        if (rd32(p + 14) != 40 || rd16(p + 26) != 1 || rd16(p + 28) != 4) continue;
        if (size < 54 || size > g_exe_size - off) continue;
        w = (int)rd32(p + 18);
        h = (int)rd32(p + 22);
        for (id = 0; id < ASSET_COUNT; id++) {
            if (g_spec[id].w != w || g_spec[id].h != h) continue;
            if (seen[id]++ == g_spec[id].nth) {
                g_bmp[id].off = off;
                g_bmp[id].size = size;
            }
        }
    }
    for (id = 0; id < ASSET_COUNT; id++)
        if (g_bmp[id].size == 0) return false;
    return true;
}

static bool try_dir(const char *dir)
{
    static const char *names[] = { "rodent.exe", "RODENT.EXE", "Rodent.exe" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char path[sizeof g_exe_path];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (read_file(path, &g_exe, &g_exe_size)) {
            snprintf(g_exe_path, sizeof g_exe_path, "%s", path);
            return true;
        }
    }
    return false;
}

bool assets_open(const char *game_dir, char *err, size_t errlen)
{
    char dirs[5][1024];
    int ndir = 0, i;
    char *base = SDL_GetBasePath();
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");

    if (game_dir) snprintf(dirs[ndir++], sizeof dirs[0], "%s", game_dir);
    if (base) {
        snprintf(dirs[ndir++], sizeof dirs[0], "%s", base);
        SDL_free(base);
    }
    snprintf(dirs[ndir++], sizeof dirs[0], ".");
    snprintf(dirs[ndir++], sizeof dirs[0], "rodents_revenge");
    if (xdg && *xdg)        snprintf(dirs[ndir++], sizeof dirs[0], "%s/rodentrecomp", xdg);
    else if (home && *home) snprintf(dirs[ndir++], sizeof dirs[0], "%s/.local/share/rodentrecomp", home);

    assets_close();
    for (i = 0; i < ndir; i++) {
        if (!try_dir(dirs[i])) continue;
        if (find_bitmaps()) return true;
        snprintf(err, errlen, "%s is not the Rodent's Revenge from Microsoft Entertainment Pack 2 "
                 "(its graphics were not found).", g_exe_path);
        assets_close();
        return false;
    }

    size_t n = (size_t)snprintf(err, errlen,
        "Rodent's Revenge needs rodent.exe from Microsoft Entertainment Pack 2.\n"
        "Put it in one of these folders, or pass --game DIR:");
    char cwd[1024];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = '\0';
    for (i = 0; i < ndir && n < errlen; i++) {
        const char *d = dirs[i];
        if (d[0] == '/' || !cwd[0])        n += (size_t)snprintf(err + n, errlen - n, "\n  %s", d);
        else if (strcmp(d, ".") == 0)      n += (size_t)snprintf(err + n, errlen - n, "\n  %s", cwd);
        else                               n += (size_t)snprintf(err + n, errlen - n, "\n  %s/%s", cwd, d);
    }
    return false;
}

void assets_close(void)
{
    free(g_exe);
    g_exe = NULL;
    g_exe_size = 0;
}

const char *assets_game_path(void) { return g_exe_path; }

void assets_set_skin(const char *skin_dir)
{
    snprintf(g_skin, sizeof g_skin, "%s", skin_dir ? skin_dir : "");
}

const char *asset_name(int id) { return g_spec[id].name; }

static SDL_Surface *load_original(int id)
{
    SDL_RWops *rw;
    if (!g_exe || g_bmp[id].size == 0) return NULL;
    rw = SDL_RWFromConstMem(g_exe + g_bmp[id].off, (int)g_bmp[id].size);
    return rw ? SDL_LoadBMP_RW(rw, 1) : NULL;
}

SDL_Surface *asset_load(int id)
{
    if (g_skin[0]) {
        char path[1200];
        SDL_Surface *s;
        snprintf(path, sizeof path, "%s/%s.png", g_skin, g_spec[id].name);
        if ((s = IMG_Load(path))) {
            if (s->w == g_spec[id].w && s->h == g_spec[id].h) return s;
            fprintf(stderr, "%s is %dx%d, expected %dx%d; using the original\n",
                    path, s->w, s->h, g_spec[id].w, g_spec[id].h);
            SDL_FreeSurface(s);
        }
    }
    return load_original(id);
}

static const char *g_readme =
    "Rodent's Revenge graphics, dumped from rodent.exe by --dump-assets.\n"
    "\n"
    "To change them, edit copies of these PNGs, put them in a folder and run\n"
    "    rodent --skin FOLDER\n"
    "Any image missing from the folder comes from rodent.exe.  Keep each\n"
    "image the same size as the original.\n"
    "\n"
    "sprites_small_*.png  108x36, tiles of 12x12 (default board)\n"
    "sprites_large_*.png  144x48, tiles of 16x16 (--large)\n"
    "timer.png            32x32 stopwatch face; the hands are drawn on top\n"
    "The *_mono sheets are the black-and-white tiles used with --mono.\n"
    "\n"
    "Sprite sheet layout: 9 columns x 3 rows.  A tile id's low hex digit is\n"
    "its column and its high digit its row.\n"
    "\n"
    "  row 0  0 open floor   1 block         2 wall\n"
    "         3 top/bottom wall where a yarn ball is coming in\n"
    "         4 left/right wall where a yarn ball is coming in\n"
    "         5 hole         6 trap          7 cheese        8 mouse in hole\n"
    "  row 1  0 cat          1-4 yarn ball (last three: fading)\n"
    "         5 life (grey mouse)            6 empty life slot\n"
    "         7 sleeping cat 8 bevel (not drawn by the game)\n"
    "  row 2  0 mouse        1-5 mouse dying (5 = dead)\n"
    "         6-8 bevels (not drawn by the game)\n";

static bool write_png(SDL_Surface *s, const char *path)
{
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
    bool ok;
    if (!rgba) return false;
    ok = stbi_write_png(path, rgba->w, rgba->h, 4, rgba->pixels, rgba->pitch) != 0;
    SDL_FreeSurface(rgba);
    return ok;
}

bool assets_dump(const char *dir)
{
    char path[1200];
    FILE *f;
    int id;

    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Cannot create %s: %s\n", dir, strerror(errno));
        return false;
    }
    for (id = 0; id < ASSET_COUNT; id++) {
        SDL_Surface *s = load_original(id);
        snprintf(path, sizeof path, "%s/%s.png", dir, g_spec[id].name);
        if (!s || !write_png(s, path)) {
            fprintf(stderr, "Cannot write %s\n", path);
            if (s) SDL_FreeSurface(s);
            return false;
        }
        SDL_FreeSurface(s);
        printf("%s\n", path);
    }
    snprintf(path, sizeof path, "%s/README.txt", dir);
    if (!(f = fopen(path, "w"))) {
        fprintf(stderr, "Cannot write %s\n", path);
        return false;
    }
    fputs(g_readme, f);
    fclose(f);
    printf("%s\n", path);
    return true;
}
