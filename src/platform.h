/*
 * platform.h - SDL2 platform abstraction layer
 *
 * Replaces the Win32 GDI calls from field100.dll and the VB1 form
 * (controls fld, pixTime, pixGuy, lbl, lbl2) with SDL2 equivalents.
 * All coordinates given to the drawing calls are in logical pixels;
 * the field's top-left corner is at platform_field_origin().
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/* Layout: a HUD strip on top, the field of cols x rows tiles below it */
bool platform_init(const char *data_dir, bool large_board, bool mono, int cols, int rows);
bool platform_mono(void);                          /* black and white display (--mono) */
bool platform_load_graphics(void);                 /* sprite sheet, after assets_open(); again after a skin change */
bool platform_configure(bool large_board, bool mono);   /* switch while running */
void platform_set_fullscreen(bool on);
void platform_show_message(const char *msg);       /* waits for a key */
void platform_shutdown(void);

/* ---- Frame ---- */
void platform_begin_frame(void);
void platform_present(void);

/* ---- Tiles ---- */
void platform_draw_tile(uint8_t tile_id, int col, int row);        /* field cell */
void platform_draw_tile_px(uint8_t tile_id, int x, int y);         /* anywhere */
void platform_fill_grid(uint8_t tile_id, int cols, int rows);
int  platform_tile_width(void);
int  platform_tile_height(void);

/* ---- Layout queries (logical pixels) ---- */
void platform_field_rect(int *x, int *y, int *w, int *h);
void platform_hud_rect(int *x, int *y, int *w, int *h);

/* ---- Primitives ---- */
void platform_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void platform_draw_line(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
void platform_draw_circle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b);
void platform_draw_image(const char *name, int x, int y, int w, int h);   /* asset_name() */

/* ---- Text ---- */
enum { FONT_NORMAL = 0, FONT_BIG = 1 };
void platform_draw_text(int font, const char *text, int x, int y, uint32_t rgb);
int  platform_text_width(int font, const char *text);
int  platform_text_height(int font);

/* ---- Window ---- */
void platform_set_title(const char *title);
void platform_save_screenshot(const char *path);   /* PNG */

/* ---- Timer ---- */
uint32_t platform_ticks(void);
void platform_delay(uint32_t ms);

/* ---- Input (VK codes as used by the VB form) ---- */
#define KEY_PGUP    0x21
#define KEY_PGDN    0x22
#define KEY_END     0x23
#define KEY_HOME    0x24
#define KEY_LEFT    0x25
#define KEY_UP      0x26
#define KEY_RIGHT   0x27
#define KEY_DOWN    0x28
#define KEY_NUM1    0x61
#define KEY_NUM9    0x69
#define KEY_F2      0x71
#define KEY_F3      0x72
#define KEY_F10     0x79
#define KEY_SPACE   0x20
#define KEY_ESCAPE  0x1B     /* also gamepad Start */
#define KEY_RETURN  0x0D     /* also gamepad A */
#define KEY_BACK    0x08     /* Backspace, gamepad B */

typedef enum { EVENT_NONE, EVENT_QUIT, EVENT_KEYDOWN } EventType;

typedef struct {
    EventType type;
    int key;       /* VK code */
    int shift;     /* 1 = Shift, 2 = Ctrl (VB Shift argument) */
} PlatformEvent;

bool platform_poll_event(PlatformEvent *event);

#endif /* PLATFORM_H */
