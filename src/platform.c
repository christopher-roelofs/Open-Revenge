/*
 * platform.c - SDL2 implementation of platform.h
 */

#include "platform.h"
#include "text.h"
#include "assets.h"
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

static SDL_Window   *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture  *g_spritesheet;
static TextFont     *g_font[2];
static char          g_data_dir[512];
static bool          g_graphics;    /* platform_load_graphics() has run */

static int g_tile_w, g_tile_h;
static int g_sheet_cols = 9, g_sheet_rows = 3;
static int g_cols, g_rows;

/* layout in logical pixels (window is SCALE x that) */
static const int SCALE  = 2;
static const int MARGIN = 4;
static int g_field_x, g_field_y, g_field_w, g_field_h;
static int g_hud_x, g_hud_y, g_hud_w, g_hud_h;
static int g_logical_w, g_logical_h;
static bool g_mono;         /* --mono: a 2-colour display */

/* HUD images (the timer), loaded on first use */
static SDL_Texture  *g_images[ASSET_COUNT];

/* gamepads (see "Gamepads" below) */
#define MAX_PADS     4
#define PAD_GRACE    40
#define PAD_DELAY    250
#define PAD_REPEAT   75
#define STICK_DEAD   16000

static SDL_GameController *g_pads[MAX_PADS];
static int      g_pad_dir;              /* numpad digit held, 0 = none */
static bool     g_pad_fired;
static uint32_t g_pad_due;              /* when to send it (again) */


/* --mono: like a 2-colour display driver, every colour becomes black or
 * white by brightness (the form's grey goes white, the red hand black) */
static void mono_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!g_mono) return;
    *r = *g = *b = (*r * 299 + *g * 587 + *b * 114) / 1000 >= 128 ? 255 : 0;
}

static SDL_Rect tile_src_rect(uint8_t tile_id)
{
    /* FLDDRAW: x = (id & 0xF) * cx, y = (id >> 4) * cy */
    SDL_Rect r = { (tile_id & 0x0F) * g_tile_w, ((tile_id >> 4) & 0x0F) * g_tile_h,
                   g_tile_w, g_tile_h };
    return r;
}

/* Layout for the board size, fonts for the display mode */
static bool configure(bool large_board, bool mono)
{
    char path[600];
    int i;

    g_tile_w = g_tile_h = large_board ? 16 : 12;
    g_mono = mono;

    /* original form: pixGuy / pixTime / score in a strip above fld */
    g_field_w = g_cols * g_tile_w; g_field_h = g_rows * g_tile_h;
    g_hud_x = MARGIN; g_hud_y = MARGIN;
    g_hud_w = g_field_w; g_hud_h = 3 * g_tile_h;
    g_field_x = MARGIN; g_field_y = g_hud_y + g_hud_h + MARGIN;
    g_logical_w = g_field_w + 2 * MARGIN;
    g_logical_h = g_field_y + g_field_h + MARGIN;
    SDL_RenderSetLogicalSize(g_renderer, g_logical_w, g_logical_h);
    if (!(SDL_GetWindowFlags(g_window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
        SDL_SetWindowSize(g_window, g_logical_w * SCALE, g_logical_h * SCALE);

    for (i = 0; i < 2; i++) { text_font_free(g_font[i]); g_font[i] = NULL; }
    snprintf(path, sizeof path, "%s/DejaVuSans.ttf", g_data_dir);
    g_font[FONT_NORMAL] = text_font_load(g_renderer, path, 13.0f, mono);
    g_font[FONT_BIG]    = text_font_load(g_renderer, path, 20.0f, mono);
    if (!g_font[FONT_NORMAL] || !g_font[FONT_BIG]) {
        fprintf(stderr, "Failed to load font: %s\n", path);
        return false;
    }
    return true;
}

bool platform_configure(bool large_board, bool mono)
{
    if (!configure(large_board, mono)) return false;
    return g_graphics ? platform_load_graphics() : true;
}

void platform_set_fullscreen(bool on)
{
    SDL_SetWindowFullscreen(g_window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!on) SDL_SetWindowSize(g_window, g_logical_w * SCALE, g_logical_h * SCALE);
}

bool platform_init(const char *data_dir, bool large_board, bool mono, int cols, int rows)
{
    /* SDL2 defaults to x11 even on Wayland sessions, where XWayland may not
     * be reachable; prefer the native driver and fall back if it fails. */
    if (getenv("WAYLAND_DISPLAY") && !getenv("SDL_VIDEODRIVER"))
        SDL_setenv("SDL_VIDEODRIVER", "wayland", 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_setenv("SDL_VIDEODRIVER", "x11", 1);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return false;
        }
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return false;
    }
    SDL_StartTextInput();
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        fprintf(stderr, "No gamepad support: %s\n", SDL_GetError());
    snprintf(g_data_dir, sizeof g_data_dir, "%s", data_dir);
    g_cols = cols; g_rows = rows;

    g_window = SDL_CreateWindow("Rodent's Revenge",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 568, 648,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        IMG_Quit(); SDL_Quit();
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer)
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window); IMG_Quit(); SDL_Quit();
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    if (!configure(large_board, mono)) {
        platform_shutdown();
        return false;
    }

    printf("Platform initialized: %dx%d logical, %dx%d tiles\n",
           g_logical_w, g_logical_h, g_tile_w, g_tile_h);
    return true;
}

void platform_shutdown(void)
{
    for (int i = 0; i < ASSET_COUNT; i++) if (g_images[i]) SDL_DestroyTexture(g_images[i]);
    for (int i = 0; i < 2; i++) text_font_free(g_font[i]);
    for (int i = 0; i < MAX_PADS; i++) if (g_pads[i]) SDL_GameControllerClose(g_pads[i]);
    if (g_spritesheet) SDL_DestroyTexture(g_spritesheet);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    IMG_Quit();
    SDL_Quit();
}

/* ---- Frame ---- */

void platform_begin_frame(void)
{
    uint8_t r = 192, g = 192, b = 192;                          /* form background */
    mono_rgb(&r, &g, &b);
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderClear(g_renderer);
}

void platform_present(void) { SDL_RenderPresent(g_renderer); }

/* ---- Tiles ---- */

void platform_draw_tile_px(uint8_t tile_id, int x, int y)
{
    if ((tile_id & 0x0F) >= g_sheet_cols || (tile_id >> 4) >= g_sheet_rows) return;
    SDL_Rect src = tile_src_rect(tile_id);
    SDL_Rect dst = { x, y, g_tile_w, g_tile_h };
    SDL_RenderCopy(g_renderer, g_spritesheet, &src, &dst);
}

void platform_draw_tile(uint8_t tile_id, int col, int row)
{
    platform_draw_tile_px(tile_id, g_field_x + col * g_tile_w, g_field_y + row * g_tile_h);
}

void platform_fill_grid(uint8_t tile_id, int cols, int rows)
{
    for (int row = 0; row < rows; row++)
        for (int col = 0; col < cols; col++)
            platform_draw_tile(tile_id, col, row);
}

int platform_tile_width(void)  { return g_tile_w; }
int platform_tile_height(void) { return g_tile_h; }

void platform_field_rect(int *x, int *y, int *w, int *h)
{
    *x = g_field_x; *y = g_field_y; *w = g_field_w; *h = g_field_h;
}

void platform_hud_rect(int *x, int *y, int *w, int *h)
{
    *x = g_hud_x; *y = g_hud_y; *w = g_hud_w; *h = g_hud_h;
}

/* ---- Primitives ---- */

void platform_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    mono_rgb(&r, &g, &b);
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(g_renderer, &rect);
}

void platform_draw_line(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    mono_rgb(&r, &g, &b);
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderDrawLine(g_renderer, x0, y0, x1, y1);
}

void platform_draw_circle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b)
{
    mono_rgb(&r, &g, &b);
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    int n = radius * 8;
    for (int i = 0; i < n; i++) {
        double a0 = 2 * M_PI * i / n, a1 = 2 * M_PI * (i + 1) / n;
        SDL_RenderDrawLine(g_renderer,
            (int)lround(cx + radius * cos(a0)), (int)lround(cy + radius * sin(a0)),
            (int)lround(cx + radius * cos(a1)), (int)lround(cy + radius * sin(a1)));
    }
}

static SDL_Texture *asset_texture(int id)
{
    SDL_Surface *s = asset_load(id);
    SDL_Texture *tex;
    if (s && g_mono) {                        /* e.g. the colour timer face */
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(s);
        s = rgba;
        if (s) {
            SDL_LockSurface(s);
            for (int y = 0; y < s->h; y++) {
                uint8_t *p = (uint8_t *)s->pixels + y * s->pitch;
                for (int x = 0; x < s->w; x++, p += 4)
                    mono_rgb(&p[0], &p[1], &p[2]);
            }
            SDL_UnlockSurface(s);
        }
    }
    tex = s ? SDL_CreateTextureFromSurface(g_renderer, s) : NULL;
    if (s) SDL_FreeSurface(s);
    return tex;
}

bool platform_load_graphics(void)
{
    bool large = g_tile_w == 16;
    for (int i = 0; i < ASSET_COUNT; i++)
        if (g_images[i]) { SDL_DestroyTexture(g_images[i]); g_images[i] = NULL; }
    if (g_spritesheet) { SDL_DestroyTexture(g_spritesheet); g_spritesheet = NULL; }
    g_graphics = true;
    /* uSize_Click: fld picture = size - 2 * fMonoG */
    g_spritesheet = asset_texture(large ? (g_mono ? ASSET_SPRITES_LARGE_MONO : ASSET_SPRITES_LARGE_COLOR)
                                        : (g_mono ? ASSET_SPRITES_SMALL_MONO : ASSET_SPRITES_SMALL_COLOR));
    if (!g_spritesheet) {
        fprintf(stderr, "Failed to load the sprite sheet: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void platform_draw_image(const char *name, int x, int y, int w, int h)
{
    SDL_Texture *tex = NULL;
    for (int id = 0; id < ASSET_COUNT; id++) {
        if (strcmp(asset_name(id), name) != 0) continue;
        if (!g_images[id]) g_images[id] = asset_texture(id);
        tex = g_images[id];
        break;
    }
    if (!tex) return;
    SDL_Rect dst = { x, y, w, h };
    SDL_RenderCopy(g_renderer, tex, NULL, &dst);
}

/* ---- Text ---- */

void platform_draw_text(int font, const char *text, int x, int y, uint32_t rgb)
{
    SDL_Color c = { (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255 };
    mono_rgb(&c.r, &c.g, &c.b);
    text_draw(g_font[font & 1], text, x, y, c);
}

int platform_text_width(int font, const char *text) { return text_width(g_font[font & 1], text); }
int platform_text_height(int font)                  { return text_line_height(g_font[font & 1]); }

/* ---- Window ---- */

void platform_set_title(const char *title) { if (g_window) SDL_SetWindowTitle(g_window, title); }

void platform_save_screenshot(const char *path)
{
    if (!g_renderer) return;
    int w, h;
    SDL_GetRendererOutputSize(g_renderer, &w, &h);
    unsigned char *px = malloc((size_t)w * h * 4);
    if (!px) return;
    if (SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_RGBA32, px, w * 4) == 0)
        stbi_write_png(path, w, h, 4, px, w * 4);
    free(px);
}

/* Draws msg word-wrapped to the window ('\n' starts a new line, long
 * words such as paths break anywhere); returns the y below it */
static int draw_wrapped(const char *msg, int x, int y, int maxw)
{
    char line[256];
    const char *p = msg;
    while (*p) {
        size_t n = 0, brk = 0, len;
        bool word = false;                          /* ignore breaks in leading spaces */
        /* grow the line a character at a time until it no longer fits */
        while (p[n] && p[n] != '\n' && n < sizeof line - 1) {
            memcpy(line, p, n + 1);
            line[n + 1] = '\0';
            if (n > 0 && platform_text_width(FONT_NORMAL, line) > maxw) break;
            if (p[n] != ' ') word = true;
            n++;
            if (p[n] == ' ' && word) brk = n;
        }
        if (!p[n] || p[n] == '\n') len = n;        /* the rest of the line fits */
        else len = brk ? brk : n;                   /* wrap at a space, else anywhere */
        memcpy(line, p, len);
        line[len] = '\0';
        text_draw(g_font[FONT_NORMAL], line, x, y, (SDL_Color){ 0, 0, 0, 255 });
        y += platform_text_height(FONT_NORMAL);
        p += len;
        if (*p == ' ' || *p == '\n') p++;
    }
    return y;
}

/* Shows msg until a key is pressed or the window is closed */
void platform_show_message(const char *msg)
{
    SDL_Event ev;
    for (;;) {
        platform_begin_frame();
        int y = draw_wrapped(msg, MARGIN, MARGIN, g_logical_w - 2 * MARGIN);
        draw_wrapped("Press any key to quit.", MARGIN, y + platform_text_height(FONT_NORMAL),
                     g_logical_w - 2 * MARGIN);
        platform_present();
        if (SDL_WaitEvent(&ev) && (ev.type == SDL_QUIT || ev.type == SDL_KEYDOWN))
            return;
    }
}

bool platform_mono(void) { return g_mono; }

/* ---- Timer ---- */

uint32_t platform_ticks(void)    { return SDL_GetTicks(); }
void platform_delay(uint32_t ms) { SDL_Delay(ms); }

/* ---- Input ---- */

static int sdl_key_to_vk(SDL_Keycode sym)
{
    switch (sym) {
    case SDLK_UP:       return KEY_UP;
    case SDLK_DOWN:     return KEY_DOWN;
    case SDLK_LEFT:     return KEY_LEFT;
    case SDLK_RIGHT:    return KEY_RIGHT;
    case SDLK_PAGEUP:   return KEY_PGUP;
    case SDLK_PAGEDOWN: return KEY_PGDN;
    case SDLK_END:      return KEY_END;
    case SDLK_HOME:     return KEY_HOME;
    case SDLK_KP_1: case SDLK_KP_2: case SDLK_KP_3: case SDLK_KP_4: case SDLK_KP_5:
    case SDLK_KP_6: case SDLK_KP_7: case SDLK_KP_8: case SDLK_KP_9:
        return KEY_NUM1 + (sym - SDLK_KP_1);
    case SDLK_F2:       return KEY_F2;
    case SDLK_F3:       return KEY_F3;
    case SDLK_F10:      return KEY_F10;
    case SDLK_SPACE:    return KEY_SPACE;
    case SDLK_ESCAPE:   return KEY_ESCAPE;
    case SDLK_RETURN: case SDLK_KP_ENTER: return KEY_RETURN;
    case SDLK_BACKSPACE: return KEY_BACK;
    default:            return 0;
    }
}

/* ---- Gamepads ----
 * The d-pad and left stick give one of 8 directions, sent as the numpad
 * keys the original used for diagonals (1-9).  A new direction waits
 * PAD_GRACE ms so two d-pad buttons pressed together arrive as a diagonal,
 * then repeats like a held key.  A = Enter, B = Backspace, Start = Esc. */

static void pad_open(int index)
{
    for (int i = 0; i < MAX_PADS; i++)
        if (!g_pads[i]) {
            g_pads[i] = SDL_GameControllerOpen(index);
            return;
        }
}

static void pad_close(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; i++)
        if (g_pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pads[i])) == id) {
            SDL_GameControllerClose(g_pads[i]);
            g_pads[i] = NULL;
        }
}

/* numpad digit for the direction held on any pad, 0 for none */
static int pad_direction(void)
{
    static const int digit[3][3] = { { 7, 8, 9 }, { 4, 0, 6 }, { 1, 2, 3 } };  /* [dy+1][dx+1] */
    int dx = 0, dy = 0;
    for (int i = 0; i < MAX_PADS; i++) {
        SDL_GameController *p = g_pads[i];
        if (!p) continue;
        dx += SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            - SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        dy += SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            - SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_UP);
        int sx = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX);
        int sy = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
        if ((long)sx * sx + (long)sy * sy > (long)STICK_DEAD * STICK_DEAD) {
            /* 8 sectors of 45 degrees: tan(22.5) ~ 0.414 */
            if (abs(sx) * 1000 > abs(sy) * 414) dx += sx > 0 ? 1 : -1;
            if (abs(sy) * 1000 > abs(sx) * 414) dy += sy > 0 ? 1 : -1;
        }
    }
    dx = dx > 0 ? 1 : dx < 0 ? -1 : 0;
    dy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    return digit[dy + 1][dx + 1];
}

static bool pad_poll(PlatformEvent *event)
{
    uint32_t now = SDL_GetTicks();
    int dir = pad_direction();

    if (dir != g_pad_dir) {
        /* a first press waits for a second button; a change while held is at once */
        g_pad_due = (g_pad_dir == 0) ? now + PAD_GRACE : now;
        g_pad_fired = false;
        g_pad_dir = dir;
    }
    if (g_pad_dir == 0 || (int32_t)(now - g_pad_due) < 0)
        return false;
    g_pad_due = now + (g_pad_fired ? PAD_REPEAT : PAD_DELAY);
    g_pad_fired = true;
    event->type = EVENT_KEYDOWN;
    event->key = KEY_NUM1 + g_pad_dir - 1;
    event->shift = 0;
    return true;
}

static int pad_button_to_vk(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:     return KEY_RETURN;
    case SDL_CONTROLLER_BUTTON_B:     return KEY_BACK;
    case SDL_CONTROLLER_BUTTON_START: return KEY_ESCAPE;
    default:                          return 0;
    }
}

bool platform_poll_event(PlatformEvent *event)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            event->type = EVENT_QUIT;
            return true;
        }
        if (ev.type == SDL_CONTROLLERDEVICEADDED) { pad_open(ev.cdevice.which); continue; }
        if (ev.type == SDL_CONTROLLERDEVICEREMOVED) { pad_close(ev.cdevice.which); continue; }
        if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
            int vk = pad_button_to_vk(ev.cbutton.button);
            if (vk) {
                event->type = EVENT_KEYDOWN;
                event->key = vk;
                event->shift = 0;
                return true;
            }
        }
        if (ev.type == SDL_TEXTINPUT) {           /* typing a high-score name */
            unsigned char c = (unsigned char)ev.text.text[0];
            if (c >= 32 && c < 127 && ev.text.text[1] == '\0') {
                event->type = EVENT_TEXT;
                event->key = c;
                event->shift = 0;
                return true;
            }
            continue;
        }
        if (ev.type == SDL_KEYDOWN) {
            int vk = sdl_key_to_vk(ev.key.keysym.sym);
            if (vk) {
                event->type = EVENT_KEYDOWN;
                event->key = vk;
                event->shift = ((ev.key.keysym.mod & KMOD_SHIFT) ? 1 : 0)
                             | ((ev.key.keysym.mod & KMOD_CTRL) ? 2 : 0);
                return true;
            }
        }
    }
    if (pad_poll(event))
        return true;
    event->type = EVENT_NONE;
    return false;
}
