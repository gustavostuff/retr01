#include "font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <string.h>

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
    if (FT_Set_Pixel_Sizes(g_ft_face, 0, R01_UI_FONT_PX) != 0) {
        FT_Done_Face(g_ft_face);
        g_ft_face = NULL;
        return -1;
    }
    return 0;
}

int font_init(void) {
    static const char *const paths[] = {
        R01_STUDIO_ASSETS_DIR "/proggy-tiny.ttf",
        "retr01_studio/assets/proggy-tiny.ttf",
        "assets/proggy-tiny.ttf",
        "../retr01_studio/assets/proggy-tiny.ttf",
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
    fprintf(stderr, "retr01_studio: failed to load proggy-tiny.ttf\n");
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
    if (font_init() != 0) {
        return R01_UI_FONT_PX;
    }
    return (int)((g_ft_face->size->metrics.height + 63) >> 6);
}

static int font_ascent(void) {
    if (font_init() != 0) {
        return R01_UI_FONT_PX - 2;
    }
    return (int)((g_ft_face->size->metrics.ascender + 63) >> 6);
}

int font_text_width(const char *text) {
    int w = 0;
    const unsigned char *p;
    if (!text || font_init() != 0) {
        return 0;
    }
    for (p = (const unsigned char *)text; *p; p++) {
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_DEFAULT) != 0) {
            continue;
        }
        w += (int)(g_ft_face->glyph->advance.x >> 6);
    }
    return w;
}

void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    int pen_x;
    int baseline;
    const unsigned char *p;

    if (!r || !text || !*text) {
        return;
    }
    if (font_init() != 0) {
        return;
    }
    pen_x = x;
    baseline = y + font_ascent();
    for (p = (const unsigned char *)text; *p; p++) {
        FT_GlyphSlot slot;
        FT_Bitmap *bm;
        int row, col;
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_RENDER) != 0) {
            continue;
        }
        slot = g_ft_face->glyph;
        bm = &slot->bitmap;
        for (row = 0; row < (int)bm->rows; row++) {
            for (col = 0; col < (int)bm->width; col++) {
                unsigned char a = bm->buffer[row * (int)bm->pitch + col];
                if (a == 0) {
                    continue;
                }
                SDL_SetRenderDrawColor(r, R, G, B, a);
                SDL_RenderDrawPoint(r, pen_x + slot->bitmap_left + col, baseline - slot->bitmap_top + row);
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
}

void font_draw_centered(SDL_Renderer *r, int x, int y, int w, int h, const char *text, Uint8 R, Uint8 G,
                        Uint8 B) {
    int tw = font_text_width(text);
    int th = font_line_h();
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2;
    font_draw(r, tx, ty, text, R, G, B);
}
