#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

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
    if (FT_Set_Pixel_Sizes(g_ft_face, 0, R01S_UI_FONT_PX) != 0) {
        FT_Done_Face(g_ft_face);
        g_ft_face = NULL;
        return -1;
    }
    return 0;
}

int font_ensure(void) {
    static const char *const paths[] = {
        R01S_ASSETS_DIR "/proggy-tiny.ttf",
        "app/assets/other/proggy-tiny.ttf",
        "../assets/other/proggy-tiny.ttf",
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
    fprintf(stderr, "retr01_sim: failed to load proggy-tiny.ttf\n");
    return -1;
}

void font_shutdown(void) {
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

int font_line_h(void) {
    if (font_ensure() != 0) {
        return R01S_UI_FONT_PX;
    }
    return (int)((g_ft_face->size->metrics.height + 63) >> 6);
}

static int font_ascent(void) {
    if (font_ensure() != 0) {
        return R01S_UI_FONT_PX - 2;
    }
    return (int)((g_ft_face->size->metrics.ascender + 63) >> 6);
}

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

void fill_rect_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_Rect rc = {x, y, w, h};
    if (A >= 255) {
        fill_rect(r, x, y, w, h, R, G, B);
        return;
    }
    if (A == 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

void font_draw_a(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    int pen_x;
    int baseline;
    const unsigned char *p;

    if (!r || !text || !*text || A == 0) {
        return;
    }
    if (font_ensure() != 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    pen_x = x;
    baseline = y + font_ascent();
    for (p = (const unsigned char *)text; *p; p++) {
        FT_GlyphSlot slot;
        FT_Bitmap *bm;
        int row, col;
        int gx, gy;

        if (*p < 0x20) {
            continue;
        }
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_RENDER) != 0) {
            continue;
        }
        slot = g_ft_face->glyph;
        bm = &slot->bitmap;
        gx = pen_x + slot->bitmap_left;
        gy = baseline - slot->bitmap_top;
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
                SDL_RenderDrawPoint(r, gx + col, gy + row);
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    font_draw_a(r, x, y, text, R, G, B, 255);
}

/* Rasterize text into an alpha buffer (tw x th), then blit. Returns 0 on success. */
static int font_raster_a(uint8_t *buf, int tw, int th, const char *text) {
    int pen_x;
    int baseline;
    const unsigned char *p;

    if (!buf || tw <= 0 || th <= 0 || !text) {
        return -1;
    }
    if (font_ensure() != 0) {
        return -1;
    }
    memset(buf, 0, (size_t)tw * (size_t)th);
    pen_x = 0;
    baseline = font_ascent();
    for (p = (const unsigned char *)text; *p; p++) {
        FT_GlyphSlot slot;
        FT_Bitmap *bm;
        int row, col;

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

int font_text_width(const char *text) {
    int w = 0;
    const unsigned char *p;

    if (!text || !*text) {
        return 0;
    }
    if (font_ensure() != 0) {
        return (int)strlen(text) * 8;
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

/* Draw text rotated 90 deg CCW (reads top->bottom). (x,y) = top-left of rotated AABB. */
void font_draw_a_rot90ccw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    int tw = font_text_width(text);
    int th = font_line_h();
    uint8_t *buf;
    int sx, sy;

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
            int dx, dy;
            if (cov == 0) {
                continue;
            }
            a = (Uint8)((cov * A) / 255);
            if (a == 0) {
                continue;
            }
            /* 90 deg CCW: (sx,sy) -> (th-1-sy, sx) */
            dx = th - 1 - sy;
            dy = sx;
            SDL_SetRenderDrawColor(r, R, G, B, a);
            SDL_RenderDrawPoint(r, x + dx, y + dy);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    free(buf);
}

int ui_label_bounce_scroll(int text_w, int view_w, unsigned phase_seed) {
    int overflow = text_w - view_w;
    Uint32 now_ms;
    Uint32 t;
    int scroll_ms;
    int pause_ms = R01S_UI_LABEL_PAUSE_MS;
    int cycle;

    if (overflow <= 0 || view_w <= 0) {
        return 0;
    }

    scroll_ms = (int)((long long)overflow * 1000 / R01S_UI_LABEL_SCROLL_PX_PER_SEC);
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

static int ui_clip_intersect(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *out) {
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

static int ui_push_clip(SDL_Renderer *r, const SDL_Rect *want, SDL_Rect *prev_out, SDL_bool *had_out) {
    SDL_Rect clip;
    SDL_bool had = SDL_RenderIsClipEnabled(r);

    *had_out = had;
    if (had) {
        SDL_RenderGetClipRect(r, prev_out);
        if (!ui_clip_intersect(prev_out, want, &clip)) {
            return 0;
        }
    } else {
        clip = *want;
    }
    SDL_RenderSetClipRect(r, &clip);
    return 1;
}

static void ui_pop_clip(SDL_Renderer *r, const SDL_Rect *prev, SDL_bool had) {
    if (had) {
        SDL_RenderSetClipRect(r, prev);
    } else {
        SDL_RenderSetClipRect(r, NULL);
    }
}

void ui_draw_label_bounce(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
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
    tw = font_text_width(text);
    fh = font_line_h();
    if (tw <= 0 || fh <= 0) {
        return;
    }
    scroll = ui_label_bounce_scroll(tw, view_w, phase_seed);
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
    if (!ui_push_clip(r, &want, &prev, &had)) {
        return;
    }
    font_draw_a(r, draw_x, draw_y, text, R, G, B, A);
    ui_pop_clip(r, &prev, had);
}

void ui_draw_label_bounce_rot90ccw(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
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
    tw = font_text_width(text);
    fh = font_line_h();
    if (tw <= 0 || fh <= 0) {
        return;
    }
    /* Rotated AABB is fh wide x tw tall. Bounce along the tall axis (view_h). */
    scroll = ui_label_bounce_scroll(tw, view_h, phase_seed);
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
    if (!ui_push_clip(r, &want, &prev, &had)) {
        return;
    }
    font_draw_a_rot90ccw(r, draw_x, draw_y, text, R, G, B, A);
    ui_pop_clip(r, &prev, had);
}

/* Draw text clipped to max_w; append "..." when truncated. Returns 1 if truncated. */
static int font_draw_ellipsize_a(SDL_Renderer *r, int x, int y, const char *text, int max_w, Uint8 R, Uint8 G,
                                 Uint8 B, Uint8 A) {
    char buf[96];
    int full_w;
    int n;
    int ell_w;

    if (!text || max_w < 4) {
        return 0;
    }
    full_w = font_text_width(text);
    if (full_w <= max_w) {
        font_draw_a(r, x, y, text, R, G, B, A);
        return 0;
    }
    ell_w = font_text_width("...");
    n = (int)strlen(text);
    if (n > (int)sizeof(buf) - 4) {
        n = (int)sizeof(buf) - 4;
    }
    while (n > 0) {
        memcpy(buf, text, (size_t)n);
        buf[n] = '\0';
        if (font_text_width(buf) + ell_w <= max_w) {
            break;
        }
        n--;
    }
    if (n <= 0) {
        font_draw_a(r, x, y, "...", R, G, B, A);
        return 1;
    }
    buf[n++] = '.';
    buf[n++] = '.';
    buf[n++] = '.';
    buf[n] = '\0';
    font_draw_a(r, x, y, buf, R, G, B, A);
    return 1;
}

int font_draw_ellipsize(SDL_Renderer *r, int x, int y, const char *text, int max_w, Uint8 R, Uint8 G,
                               Uint8 B) {
    return font_draw_ellipsize_a(r, x, y, text, max_w, R, G, B, 255);
}
