#include "font/font.h"

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

#ifndef R01_STUDIO_FONT_DIR
#define R01_STUDIO_FONT_DIR "retr01/assets/other"
#endif

int font_init(void) {
    static const char *const paths[] = {
        R01_STUDIO_FONT_DIR "/proggy-tiny.ttf",
        "retr01/assets/other/proggy-tiny.ttf",
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
    return font_text_width_n(text, -1);
}

int font_text_width_n(const char *text, int n) {
    int w = 0;
    int i;
    const unsigned char *p;
    if (!text || font_init() != 0) {
        return 0;
    }
    for (p = (const unsigned char *)text, i = 0; *p && (n < 0 || i < n); p++, i++) {
        if (FT_Load_Char(g_ft_face, (FT_ULong)*p, FT_LOAD_DEFAULT) != 0) {
            continue;
        }
        w += (int)(g_ft_face->glyph->advance.x >> 6);
    }
    return w;
}

static void font_draw_internal(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B,
                               Uint8 alpha) {
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
                unsigned int aa;
                if (a == 0) {
                    continue;
                }
                aa = ((unsigned int)a * (unsigned int)alpha) / 255u;
                if (aa == 0) {
                    continue;
                }
                SDL_SetRenderDrawColor(r, R, G, B, (Uint8)aa);
                SDL_RenderDrawPoint(r, pen_x + slot->bitmap_left + col, baseline - slot->bitmap_top + row);
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
}

void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    font_draw_internal(r, x, y, text, R, G, B, 255);
}

void font_draw_sized_alpha(SDL_Renderer *r, int x, int y, int px, const char *text, Uint8 R, Uint8 G, Uint8 B,
                           Uint8 alpha) {
    if (font_init() != 0) {
        return;
    }
    if (px < 4) {
        px = 4;
    }
    if (FT_Set_Pixel_Sizes(g_ft_face, 0, (FT_UInt)px) != 0) {
        return;
    }
    font_draw_internal(r, x, y, text, R, G, B, alpha);
    FT_Set_Pixel_Sizes(g_ft_face, 0, R01_UI_FONT_PX);
}

void font_draw_centered(SDL_Renderer *r, int x, int y, int w, int h, const char *text, Uint8 R, Uint8 G,
                        Uint8 B) {
    int tw = font_text_width(text);
    int th = font_line_h();
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2;
    font_draw(r, tx, ty, text, R, G, B);
}

static int font_wrap_next_line(const char *text, int max_w, int *out_len) {
    int i;
    int last_space = -1;
    int w = 0;
    if (!text || !text[0] || max_w < 1) {
        if (out_len) {
            *out_len = 0;
        }
        return 0;
    }
    for (i = 0; text[i]; i++) {
        char ch = text[i];
        int cw;
        if (ch == '\n') {
            if (out_len) {
                *out_len = i;
            }
            return 1; /* caller advances past newline */
        }
        if (ch == ' ') {
            last_space = i;
        }
        cw = font_text_width_n(&ch, 1);
        if (w + cw > max_w && i > 0) {
            if (last_space > 0) {
                if (out_len) {
                    *out_len = last_space;
                }
                return 1;
            }
            if (out_len) {
                *out_len = i;
            }
            return 1;
        }
        w += cw;
    }
    if (out_len) {
        *out_len = i;
    }
    return 0;
}

int font_measure_wrapped(const char *text, int max_w) {
    const char *p = text;
    int lines = 0;
    int lh = font_line_h();
    if (!text || !text[0]) {
        return 0;
    }
    if (max_w < 1) {
        max_w = 1;
    }
    while (*p) {
        int len = 0;
        int more = font_wrap_next_line(p, max_w, &len);
        lines++;
        p += len;
        if (*p == '\n' || (*p == ' ' && more)) {
            p++;
        }
        if (!more && !*p) {
            break;
        }
        if (!more) {
            break;
        }
    }
    if (lines < 1) {
        lines = 1;
    }
    return lines * lh;
}

int font_draw_wrapped(SDL_Renderer *r, int x, int y, int max_w, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    const char *p = text;
    int yy = y;
    int lh = font_line_h();
    char line[256];
    if (!r || !text || !text[0]) {
        return 0;
    }
    if (max_w < 1) {
        max_w = 1;
    }
    while (*p) {
        int len = 0;
        int more = font_wrap_next_line(p, max_w, &len);
        if (len < 0) {
            len = 0;
        }
        if (len >= (int)sizeof(line)) {
            len = (int)sizeof(line) - 1;
        }
        memcpy(line, p, (size_t)len);
        line[len] = '\0';
        font_draw(r, x, yy, line, R, G, B);
        yy += lh;
        p += len;
        if (*p == '\n' || (*p == ' ' && more)) {
            p++;
        }
        if (!more) {
            break;
        }
    }
    return yy - y;
}
