/*
 * field.c - Rodent's Revenge field (tile grid)
 *
 * Reconstructed from field100.dll; GDI calls replaced by platform calls.
 */

#include "field.h"
#include "platform.h"
#include <string.h>

Field g_field;

void field_init(int cols, int rows)
{
    if (cols > FIELD_MAX) cols = FIELD_MAX;
    if (rows > FIELD_MAX) rows = FIELD_MAX;
    g_field.cols = cols;
    g_field.rows = rows;
    memset(g_field.tile, 0, sizeof(g_field.tile));
    g_field.initialized = true;
}

void field_free(void)
{
    g_field.initialized = false;
}

static bool in_range(int x, int y)
{
    return x >= 0 && y >= 0 && x < g_field.cols && y < g_field.rows;
}

int field_get(int x, int y)
{
    if (!in_range(x, y)) return 0;
    return g_field.tile[x][y];
}

void field_draw(uint8_t tile_id, int x, int y)
{
    if (!in_range(x, y)) return;
    g_field.tile[x][y] = tile_id;
    platform_draw_tile(tile_id, x, y);
}

void field_erase(uint8_t tile_id)
{
    if (!g_field.initialized) return;
    for (int x = 0; x < g_field.cols; x++)
        for (int y = 0; y < g_field.rows; y++)
            g_field.tile[x][y] = tile_id;
    platform_fill_grid(tile_id, g_field.cols, g_field.rows);
}

void field_paint(void)
{
    if (!g_field.initialized) return;
    for (int y = 0; y < g_field.rows; y++)
        for (int x = 0; x < g_field.cols; x++)
            platform_draw_tile(g_field.tile[x][y], x, y);
}

/*
 * FLDMELT: the original shifts random-height column slices of the client
 * area downwards one column at a time, left to right, until the screen is
 * blank (interruptible by a key press).  Approximated here by wiping the
 * columns left to right.
 */
void field_melt(void)
{
    if (!g_field.initialized) return;
    for (int x = 0; x < g_field.cols; x++) {
        for (int y = 0; y < g_field.rows; y++)
            platform_draw_tile(0, x, y);
        platform_present();
        platform_delay(15);
    }
}
