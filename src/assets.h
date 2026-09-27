/*
 * assets.h - game graphics, read straight from the player's rodent.exe
 *
 * VB1 stores each PictureBox/Form picture as a complete .bmp file inside
 * the form data, so the bitmaps are found by scanning rodent.exe for "BM"
 * headers of the right size.  A skin directory can override any of them
 * with <name>.png; --dump-assets writes them out under those names.
 */

#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>
#include <stddef.h>

struct SDL_Surface;

enum {
    ASSET_SPRITES_SMALL_COLOR,   /* 108x36: 9x3 tiles of 12x12 */
    ASSET_SPRITES_SMALL_MONO,
    ASSET_SPRITES_LARGE_COLOR,   /* 144x48: 9x3 tiles of 16x16 */
    ASSET_SPRITES_LARGE_MONO,
    ASSET_TIMER,                 /* 32x32 stopwatch face (pixTime) */
    ASSET_COUNT
};

/* Finds and reads rodent.exe: game_dir first (if not NULL), then the
 * standard places.  On failure, err describes where it looked. */
bool assets_open(const char *game_dir, char *err, size_t errlen);
void assets_close(void);

/* Where rodent.exe was found (valid after assets_open) */
const char *assets_game_path(void);

/* Images an override in skin_dir (NULL for none) replaces: <name>.png */
void assets_set_skin(const char *skin_dir);

const char *asset_name(int id);                 /* e.g. "sprites_small_color" */
struct SDL_Surface *asset_load(int id);         /* caller frees; NULL on error */

/* Writes every asset as <dir>/<name>.png plus a README.txt for modders */
bool assets_dump(const char *dir);

#endif /* ASSETS_H */
