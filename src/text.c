/*
 * text.c - stb_truetype glyph cache on an SDL_Texture atlas.
 *
 * Glyphs are rasterised on demand into a 512x512 RGBA atlas (shelf packer)
 * and drawn with SDL_RenderCopy, so text scales with the logical size like
 * the tiles do.
 */
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_W 512
#define ATLAS_H 512
#define MAX_GLYPHS 512

typedef struct {
    unsigned cp;
    SDL_Rect src;
    int xoff, yoff;
    float xadv;
} Glyph;

struct TextFont {
    SDL_Renderer *r;
    unsigned char *ttf;
    stbtt_fontinfo info;
    float scale, ascent, line_h;
    SDL_Texture *atlas;
    int pen_x, pen_y, row_h;
    Glyph glyphs[MAX_GLYPHS];
    int nglyphs;
    bool hard;          /* 1-bit glyphs, no anti-aliasing (--mono) */
};

static unsigned char *read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *buf = malloc(n);
    if (buf && fread(buf, 1, n, fp) != (size_t)n) { free(buf); buf = NULL; }
    fclose(fp);
    return buf;
}

TextFont *text_font_load(SDL_Renderer *r, const char *ttf_path, float pixel_h, bool hard_edges)
{
    TextFont *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    f->r = r;
    f->hard = hard_edges;
    f->ttf = read_file(ttf_path);
    if (!f->ttf || !stbtt_InitFont(&f->info, f->ttf, stbtt_GetFontOffsetForIndex(f->ttf, 0))) {
        text_font_free(f);
        return NULL;
    }
    f->scale = stbtt_ScaleForPixelHeight(&f->info, pixel_h);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    f->ascent = asc * f->scale;
    f->line_h = (asc - desc + gap) * f->scale;
    f->atlas = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
    if (!f->atlas) { text_font_free(f); return NULL; }
    SDL_SetTextureBlendMode(f->atlas, SDL_BLENDMODE_BLEND);
    /* start with a transparent atlas */
    unsigned char *zero = calloc(ATLAS_W * ATLAS_H, 4);
    SDL_UpdateTexture(f->atlas, NULL, zero, ATLAS_W * 4);
    free(zero);
    f->pen_x = f->pen_y = 1;
    return f;
}

void text_font_free(TextFont *f)
{
    if (!f) return;
    if (f->atlas) SDL_DestroyTexture(f->atlas);
    free(f->ttf);
    free(f);
}

static unsigned utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned cp = *p;
    int n = 0;
    if (cp >= 0xF0)      { cp &= 0x07; n = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; n = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; n = 1; }
    p++;
    while (n-- > 0 && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
    *s = (const char *)p;
    return cp;
}

static Glyph *glyph(TextFont *f, unsigned cp)
{
    for (int i = 0; i < f->nglyphs; i++)
        if (f->glyphs[i].cp == cp) return &f->glyphs[i];
    if (f->nglyphs >= MAX_GLYPHS) return NULL;

    int gi = stbtt_FindGlyphIndex(&f->info, (int)cp);
    int adv, lsb, x0, y0, x1, y1;
    stbtt_GetGlyphHMetrics(&f->info, gi, &adv, &lsb);
    stbtt_GetGlyphBitmapBox(&f->info, gi, f->scale, f->scale, &x0, &y0, &x1, &y1);
    int gw = x1 - x0, gh = y1 - y0;

    if (f->pen_x + gw + 1 > ATLAS_W) { f->pen_x = 1; f->pen_y += f->row_h + 1; f->row_h = 0; }
    if (f->pen_y + gh + 1 > ATLAS_H) return NULL;

    if (gw > 0 && gh > 0) {
        unsigned char *mono = malloc(gw * gh);
        unsigned char *rgba = malloc(gw * gh * 4);
        stbtt_MakeGlyphBitmap(&f->info, mono, gw, gh, gw, f->scale, f->scale, gi);
        for (int i = 0; i < gw * gh; i++) {
            rgba[i*4+0] = rgba[i*4+1] = rgba[i*4+2] = 255;
            rgba[i*4+3] = f->hard ? (mono[i] >= 128 ? 255 : 0) : mono[i];
        }
        SDL_Rect dst = { f->pen_x, f->pen_y, gw, gh };
        SDL_UpdateTexture(f->atlas, &dst, rgba, gw * 4);
        free(mono); free(rgba);
    }
    Glyph *g = &f->glyphs[f->nglyphs++];
    g->cp = cp;
    g->src = (SDL_Rect){ f->pen_x, f->pen_y, gw, gh };
    g->xoff = x0; g->yoff = y0;
    g->xadv = adv * f->scale;
    if (gh > f->row_h) f->row_h = gh;
    f->pen_x += gw + 1;
    return g;
}

int text_width(TextFont *f, const char *s)
{
    float w = 0;
    while (*s) {
        Glyph *g = glyph(f, utf8_next(&s));
        if (g) w += g->xadv;
    }
    return (int)(w + 0.5f);
}

int text_line_height(TextFont *f) { return (int)(f->line_h + 0.5f); }

void text_draw(TextFont *f, const char *s, int x, int y, SDL_Color c)
{
    float pen = (float)x;
    int base = y + (int)(f->ascent + 0.5f);
    SDL_SetTextureColorMod(f->atlas, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(f->atlas, c.a);
    while (*s) {
        Glyph *g = glyph(f, utf8_next(&s));
        if (!g) continue;
        if (g->src.w > 0) {
            SDL_Rect dst = { (int)(pen + g->xoff), base + g->yoff, g->src.w, g->src.h };
            SDL_RenderCopy(f->r, f->atlas, &g->src, &dst);
        }
        pen += g->xadv;
    }
}
