/*
 * field.h - Rodent's Revenge field (tile grid)
 *
 * Reconstructed from field100.dll.  The game only uses FldErase, FldDraw,
 * FldDrawOn, FldGet and FldMelt (see docs/GAME_LOGIC.md); the wall
 * smoothing/template exports of the DLL are never called by rodent.exe.
 *
 * Tile ids encode the sprite sheet position: low nibble = column,
 * high nibble = row (FLDDRAW blits from x = (id & 0xF) * cx,
 * y = (id >> 4) * cy).
 */

#ifndef FIELD_H
#define FIELD_H

#include <stdint.h>
#include <stdbool.h>

#define FIELD_MAX 64

typedef struct {
    int     cols;
    int     rows;
    uint8_t tile[FIELD_MAX][FIELD_MAX];   /* tile[x][y] */
    bool    initialized;
} Field;

extern Field g_field;

/* VBINITCC / Width,Height properties: allocate a cols x rows grid */
void field_init(int cols, int rows);
void field_free(void);

/* FLDDRAW: store tile at (x, y) and blit it */
void field_draw(uint8_t tile_id, int x, int y);

/* FLDGET: tile at (x, y), 0 when out of range */
int field_get(int x, int y);

/* FLDERASE: fill the whole grid with a tile */
void field_erase(uint8_t tile_id);

/* WM_PAINT: blit the whole grid */
void field_paint(void);

/* FLDMELT: dissolve effect */
void field_melt(void);

#endif /* FIELD_H */
