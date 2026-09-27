/*
 * text.h - TrueType text for the SDL renderer (stb_truetype glyph atlas,
 * same approach as ytc's gfx::Font but in C and on SDL_Renderer).
 */
#ifndef TEXT_H
#define TEXT_H

#include <SDL.h>
#include <stdbool.h>

typedef struct TextFont TextFont;

/* Load a .ttf at the given pixel height; hard_edges turns off
 * anti-aliasing (1-bit glyphs). NULL on failure. */
TextFont *text_font_load(SDL_Renderer *r, const char *ttf_path, float pixel_h, bool hard_edges);
void      text_font_free(TextFont *f);

int  text_width(TextFont *f, const char *s);
int  text_line_height(TextFont *f);

/* Draw UTF-8/ASCII text with its top-left corner at (x, y). */
void text_draw(TextFont *f, const char *s, int x, int y, SDL_Color c);

#endif /* TEXT_H */
