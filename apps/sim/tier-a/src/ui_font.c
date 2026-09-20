#include "ui_font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef R01A_ASSETS_OTHER
#define R01A_ASSETS_OTHER "assets/other"
#endif

static FT_Library g_ft_lib;
static FT_Face g_ft_face;
static int g_ft_ready;

static int font_try_open(const char *path) {
    if (!path || !path[0]) {
        return -1;
    }
    if (FT_New_Face(g_ft_lib, path, 0, &g_ft_face) != 0) {
        return -1;
    }
    if (FT_Set_Pixel_Sizes(g_ft_face, 0, R01A_UI_FONT_PX) != 0) {
        FT_Done_Face(g_ft_face);
        g_ft_face = NULL;
        return -1;
    }
    return 0;
}

int r01a_font_init(void) {
    static const char *const paths[] = {
        R01A_ASSETS_OTHER "/proggy-tiny.ttf",
        "apps/sim/tier-a/assets/other/proggy-tiny.ttf",
        "assets/other/proggy-tiny.ttf",
        NULL,
    };
    int i;

    if (g_ft_ready) {
        return 0;
    }
    if (FT_Init_FreeType(&g_ft_lib) != 0) {
        return -1;
    }
    for (i = 0; paths[i]; i++) {
        if (font_try_open(paths[i]) == 0) {
            g_ft_ready = 1;
            return 0;
        }
    }
    FT_Done_FreeType(g_ft_lib);
    g_ft_lib = NULL;
    fprintf(stderr, "retr01_sim_tier_a: failed to load proggy-tiny.ttf\n");
    return -1;
}

void r01a_font_shutdown(void) {
    if (g_ft_face) {
        FT_Done_Face(g_ft_face);
        g_ft_face = NULL;
    }
    if (g_ft_lib) {
        FT_Done_FreeType(g_ft_lib);
        g_ft_lib = NULL;
    }
    g_ft_ready = 0;
}

int r01a_font_line_h(void) {
    if (r01a_font_init() != 0) {
        return R01A_UI_FONT_PX;
    }
    return (int)((g_ft_face->size->metrics.height + 63) >> 6);
}

static int font_ascent(void) {
    if (r01a_font_init() != 0) {
        return R01A_UI_FONT_PX - 2;
    }
    return (int)((g_ft_face->size->metrics.ascender + 63) >> 6);
}

int r01a_font_text_width(const char *text) {
    int w = 0;
    const unsigned char *p;

    if (!text || r01a_font_init() != 0) {
        return 0;
    }
    for (p = (const unsigned char *)text; *p; p++) {
        if (*p < 0x20) {
            continue;
        }
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_DEFAULT) != 0) {
            continue;
        }
        w += (int)(g_ft_face->glyph->advance.x >> 6);
    }
    return w;
}

void r01a_font_draw_a(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    int pen_x;
    int baseline;
    const unsigned char *p;

    if (!r || !text || !*text || A == 0) {
        return;
    }
    if (r01a_font_init() != 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    pen_x = x;
    baseline = y + font_ascent();
    for (p = (const unsigned char *)text; *p; p++) {
        FT_GlyphSlot slot;
        FT_Bitmap *bm;
        int row;
        int col;

        if (*p < 0x20) {
            continue;
        }
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_RENDER) != 0) {
            continue;
        }
        slot = g_ft_face->glyph;
        bm = &slot->bitmap;
        for (row = 0; row < (int)bm->rows; row++) {
            for (col = 0; col < (int)bm->width; col++) {
                Uint8 cov = bm->buffer[row * (int)bm->pitch + col];
                Uint8 a;
                if (cov == 0) {
                    continue;
                }
                a = (Uint8)((cov * A) / 255);
                if (a == 0) {
                    continue;
                }
                SDL_SetRenderDrawColor(r, R, G, B, a);
                SDL_RenderDrawPoint(r, pen_x + slot->bitmap_left + col, baseline - slot->bitmap_top + row);
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void r01a_font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    r01a_font_draw_a(r, x, y, text, R, G, B, 255);
}

static int font_raster_a(uint8_t *buf, int tw, int th, const char *text) {
    int pen_x;
    int baseline;
    const unsigned char *p;

    if (!buf || tw <= 0 || th <= 0 || !text) {
        return -1;
    }
    if (r01a_font_init() != 0) {
        return -1;
    }
    memset(buf, 0, (size_t)tw * (size_t)th);
    pen_x = 0;
    baseline = font_ascent();
    for (p = (const unsigned char *)text; *p; p++) {
        FT_GlyphSlot slot;
        FT_Bitmap *bm;
        int row;
        int col;

        if (*p < 0x20) {
            continue;
        }
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_RENDER) != 0) {
            continue;
        }
        slot = g_ft_face->glyph;
        bm = &slot->bitmap;
        for (row = 0; row < (int)bm->rows; row++) {
            int gy = baseline - slot->bitmap_top + row;
            if (gy < 0 || gy >= th) {
                continue;
            }
            for (col = 0; col < (int)bm->width; col++) {
                int gx = pen_x + slot->bitmap_left + col;
                Uint8 cov;
                if (gx < 0 || gx >= tw) {
                    continue;
                }
                cov = bm->buffer[row * (int)bm->pitch + col];
                if (cov > buf[gy * tw + gx]) {
                    buf[gy * tw + gx] = cov;
                }
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
    return 0;
}

static void font_draw_a_rot90ccw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B,
                                 Uint8 A) {
    int tw = r01a_font_text_width(text);
    int th = r01a_font_line_h();
    uint8_t *buf;
    int sx;
    int sy;

    if (!r || !text || !*text || A == 0 || tw <= 0 || th <= 0) {
        return;
    }
    buf = (uint8_t *)calloc((size_t)tw * (size_t)th, 1);
    if (!buf) {
        return;
    }
    if (font_raster_a(buf, tw, th, text) != 0) {
        free(buf);
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (sy = 0; sy < th; sy++) {
        for (sx = 0; sx < tw; sx++) {
            Uint8 cov = buf[sy * tw + sx];
            Uint8 a;
            if (cov == 0) {
                continue;
            }
            a = (Uint8)((cov * A) / 255);
            if (a == 0) {
                continue;
            }
            SDL_SetRenderDrawColor(r, R, G, B, a);
            /* 90 CCW: (sx,sy) -> (sy, tw-1-sx) */
            SDL_RenderDrawPoint(r, x + sy, y + (tw - 1 - sx));
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    free(buf);
}

static int label_bounce_scroll(int text_w, int view_w, unsigned phase_seed) {
    int overflow = text_w - view_w;
    Uint32 now_ms;
    Uint32 t;
    int scroll_ms;
    int pause_ms = R01A_UI_LABEL_PAUSE_MS;
    int cycle;

    if (overflow <= 0 || view_w <= 0) {
        return 0;
    }
    scroll_ms = (int)((long long)overflow * 1000 / R01A_UI_LABEL_SCROLL_PX_PER_SEC);
    if (scroll_ms < 1) {
        scroll_ms = 1;
    }
    cycle = pause_ms + scroll_ms + pause_ms + scroll_ms;
    now_ms = SDL_GetTicks();
    t = (now_ms + phase_seed) % (Uint32)cycle;
    if (t < (Uint32)pause_ms) {
        return 0;
    }
    t -= (Uint32)pause_ms;
    if (t < (Uint32)scroll_ms) {
        return (int)((long long)t * overflow / scroll_ms);
    }
    t -= (Uint32)scroll_ms;
    if (t < (Uint32)pause_ms) {
        return overflow;
    }
    t -= (Uint32)pause_ms;
    return overflow - (int)((long long)t * overflow / scroll_ms);
}

static int clip_intersect(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *out) {
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static int push_clip(SDL_Renderer *r, const SDL_Rect *want, SDL_Rect *prev_out, SDL_bool *had_out) {
    SDL_Rect clip;
    SDL_bool had = SDL_RenderIsClipEnabled(r);

    *had_out = had;
    if (had) {
        SDL_RenderGetClipRect(r, prev_out);
        if (!clip_intersect(prev_out, want, &clip)) {
            return 0;
        }
    } else {
        clip = *want;
    }
    SDL_RenderSetClipRect(r, &clip);
    return 1;
}

static void pop_clip(SDL_Renderer *r, const SDL_Rect *prev, SDL_bool had) {
    if (had) {
        SDL_RenderSetClipRect(r, prev);
    } else {
        SDL_RenderSetClipRect(r, NULL);
    }
}

void r01a_draw_label_bounce(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                            unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    int tw;
    int fh;
    int scroll;
    int draw_x;
    int draw_y;
    SDL_Rect want;
    SDL_Rect prev;
    SDL_bool had;

    if (!r || !text || !text[0] || view_w <= 0 || view_h <= 0 || A == 0) {
        return;
    }
    tw = r01a_font_text_width(text);
    fh = r01a_font_line_h();
    if (tw <= 0 || fh <= 0) {
        return;
    }
    scroll = label_bounce_scroll(tw, view_w, phase_seed);
    if (tw <= view_w) {
        draw_x = x + (view_w - tw) / 2;
    } else {
        draw_x = x - scroll;
    }
    draw_y = y + (view_h - fh) / 2;
    want.x = x;
    want.y = y;
    want.w = view_w;
    want.h = view_h;
    if (!push_clip(r, &want, &prev, &had)) {
        return;
    }
    r01a_font_draw_a(r, draw_x, draw_y, text, R, G, B, A);
    pop_clip(r, &prev, had);
}

void r01a_draw_label_bounce_rot90ccw(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                                     unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    int tw;
    int fh;
    int scroll;
    int draw_x;
    int draw_y;
    SDL_Rect want;
    SDL_Rect prev;
    SDL_bool had;

    if (!r || !text || !text[0] || view_w <= 0 || view_h <= 0 || A == 0) {
        return;
    }
    tw = r01a_font_text_width(text);
    fh = r01a_font_line_h();
    if (tw <= 0 || fh <= 0) {
        return;
    }
    scroll = label_bounce_scroll(tw, view_h, phase_seed);
    draw_x = x + (view_w - fh) / 2;
    if (tw <= view_h) {
        draw_y = y + (view_h - tw) / 2;
    } else {
        draw_y = y - scroll;
    }
    want.x = x;
    want.y = y;
    want.w = view_w;
    want.h = view_h;
    if (!push_clip(r, &want, &prev, &had)) {
        return;
    }
    font_draw_a_rot90ccw(r, draw_x, draw_y, text, R, G, B, A);
    pop_clip(r, &prev, had);
}
