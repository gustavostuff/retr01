#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *g_radio_rgba;
int g_radio_w;
int g_radio_h;
uint8_t *g_dot_rgba;
int g_dot_w;
int g_dot_h;
uint8_t *g_checkbox_rgba;
int g_checkbox_w;
int g_checkbox_h;
uint8_t *g_cross_rgba;
int g_cross_w;
int g_cross_h;

int ui_load_png_rgba(const char *path, uint8_t **out_px, int *out_w, int *out_h) {
    FILE *fp;
    png_structp png;
    png_infop info;
    png_byte header[8];
    png_bytep *rows = NULL;
    uint8_t *px;
    int w, h, y;
    if (!path || !out_px || !out_w || !out_h) {
        return -1;
    }
    px = NULL;
    rows = NULL;
    fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8) != 0) {
        fclose(fp);
        return -1;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    if (!png || !info) {
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(px);
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);
    w = (int)png_get_image_width(png, info);
    h = (int)png_get_image_height(png, info);
    png_set_expand(png);
    png_read_update_info(png, info);
    px = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    rows = (png_bytep *)malloc((size_t)h * sizeof(png_bytep));
    if (!px || !rows) {
        free(rows);
        free(px);
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    for (y = 0; y < h; y++) {
        rows[y] = px + (size_t)y * (size_t)w * 4u;
    }
    png_read_image(png, rows);
    png_read_end(png, NULL);
    free(rows);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);
    *out_px = px;
    *out_w = w;
    *out_h = h;
    return 0;
}
int snap8(int v) {
    if (v < UI_UNIT) {
        return UI_UNIT;
    }
    return (v + UI_UNIT - 1) & ~(UI_UNIT - 1);
}

void ui_toast(UiState *ui, const char *msg, int is_error) {
    if (!ui || !msg) {
        return;
    }
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_error = is_error;
    ui->toast_until = SDL_GetTicks() + UI_TOAST_MS;
}

void ui_tooltip_set(UiState *ui, int x, int y, const char *line1, const char *line2) {
    if (!ui) {
        return;
    }
    ui->tooltip_active = 1;
    ui->tooltip_x = x;
    ui->tooltip_y = y;
    if (line2 && line2[0]) {
        snprintf(ui->tooltip, sizeof(ui->tooltip), "%s\n%s", line1 ? line1 : "", line2);
    } else {
        snprintf(ui->tooltip, sizeof(ui->tooltip), "%s", line1 ? line1 : "");
    }
}

void ui_tooltip_clear(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->tooltip_active = 0;
    ui->tooltip[0] = '\0';
}

void draw_tooltip(UiState *ui, SDL_Renderer *r) {
    int tw, th, x, y;
    const char *nl;
    char line1[160];
    char line2[160];
    if (!ui || !ui->tooltip_active || !ui->tooltip[0]) {
        return;
    }
    line1[0] = '\0';
    line2[0] = '\0';
    nl = strchr(ui->tooltip, '\n');
    if (nl) {
        size_t n = (size_t)(nl - ui->tooltip);
        if (n >= sizeof(line1)) {
            n = sizeof(line1) - 1;
        }
        memcpy(line1, ui->tooltip, n);
        line1[n] = '\0';
        snprintf(line2, sizeof(line2), "%s", nl + 1);
    } else {
        snprintf(line1, sizeof(line1), "%s", ui->tooltip);
    }
    tw = label_width(line1);
    if (line2[0]) {
        int w2 = label_width(line2);
        if (w2 > tw) {
            tw = w2;
        }
    }
    th = line2[0] ? (UI_BTN_H * 2) : UI_BTN_H;
    x = ui->tooltip_x + UI_UNIT;
    y = ui->tooltip_y + UI_UNIT;
    if (x + tw > ui_logic_w(ui) - UI_UNIT) {
        x = ui_logic_w(ui) - UI_UNIT - tw;
    }
    if (y + th > ui_logic_h(ui) - UI_UNIT) {
        y = ui->tooltip_y - th - 2;
    }
    if (x < UI_UNIT) {
        x = UI_UNIT;
    }
    if (y < UI_UNIT) {
        y = UI_UNIT;
    }
    fill_rect(r, x, y, tw, th, 24, 24, 30);
    draw_rect(r, x, y, tw, th, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    font_draw(r, x + 4, y + 4, line1, 240, 240, 240);
    if (line2[0]) {
        font_draw(r, x + 4, y + UI_BTN_H + 4, line2, 180, 180, 190);
    }
}

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static SDL_Rect ui_clip_intersect(const SDL_Rect *a, const SDL_Rect *b) {
    SDL_Rect out;
    int x1;
    int y1;
    int x2;
    int y2;

    x1 = a->x > b->x ? a->x : b->x;
    y1 = a->y > b->y ? a->y : b->y;
    x2 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
    y2 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    out.x = x1;
    out.y = y1;
    out.w = x2 - x1;
    out.h = y2 - y1;
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

void ui_clip_push(SDL_Renderer *r, int x, int y, int w, int h, UiClipStack *stack) {
    SDL_Rect next = {x, y, w, h};
    SDL_Rect prev;

    if (!r || !stack || w < 1 || h < 1) {
        return;
    }
    stack->had_clip = SDL_FALSE;
    SDL_RenderGetClipRect(r, &prev);
    if (prev.w > 0 && prev.h > 0) {
        stack->had_clip = SDL_TRUE;
        stack->prev = prev;
        next = ui_clip_intersect(&prev, &next);
    }
    SDL_RenderSetClipRect(r, &next);
}

void ui_clip_pop(SDL_Renderer *r, const UiClipStack *stack) {
    if (!r || !stack) {
        return;
    }
    if (stack->had_clip) {
        SDL_RenderSetClipRect(r, &stack->prev);
    } else {
        SDL_RenderSetClipRect(r, NULL);
    }
}

void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 77);
    SDL_RenderFillRect(r, &rc);
}

void font_draw_clipped(SDL_Renderer *r, int x, int y, int clip_x, int clip_y, int clip_w, int clip_h,
                       const char *text, Uint8 R, Uint8 G, Uint8 B) {
    UiClipStack stack;
    if (clip_w < 1 || clip_h < 1 || !text) {
        return;
    }
    ui_clip_push(r, clip_x, clip_y, clip_w, clip_h, &stack);
    font_draw(r, x, y, text, R, G, B);
    ui_clip_pop(r, &stack);
}

int point_in_rect(int lx, int ly, int x, int y, int w, int h) {
    return lx >= x && ly >= y && lx < x + w && ly < y + h;
}

int label_width(const char *text) {
    return snap8(font_text_width(text) + UI_UNIT);
}

void draw_brush_preview(SDL_Renderer *r, const R01Project *p, int row, int pal, int color, int mx, int my) {
    uint8_t cr, cg, cb;
    if (!p || !r) {
        return;
    }
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    if (pal < 0 || pal >= R01_PALS_PER_ROW) {
        pal = 0;
    }
    if (color < 0 || color >= R01_PAL_COLORS) {
        color = 0;
    }
    r01_kit_rgb(p->global_pal_spr[row][pal].idx[color & 3u], &cr, &cg, &cb);
    fill_rect(r, mx + 10, my + 10, 8, 8, cr, cg, cb);
    draw_rect(r, mx + 10, my + 10, 8, 8, 200, 200, 200);
}

void draw_ui_cross(SDL_Renderer *r, int cx, int cy) {
    int px, py;
    int ox = cx - UI_DOT_SIZE / 2;
    int oy = cy - UI_DOT_SIZE / 2;
    if (g_cross_rgba && g_cross_w == UI_DOT_SIZE && g_cross_h == UI_DOT_SIZE) {
        for (py = 0; py < UI_DOT_SIZE; py++) {
            for (px = 0; px < UI_DOT_SIZE; px++) {
                const uint8_t *p = &g_cross_rgba[(py * g_cross_w + px) * 4u];
                if (p[3] > 128) {
                    fill_rect(r, ox + px, oy + py, 1, 1, 240, 240, 240);
                }
            }
        }
        return;
    }
    fill_rect(r, cx - 3, cy, 7, 1, 240, 240, 240);
    fill_rect(r, cx, cy - 3, 1, 7, 240, 240, 240);
}

void draw_label(SDL_Renderer *r, int x, int y, const char *text) {
    int w = label_width(text);
    font_draw_centered(r, x, y, w, UI_BTN_H, text, 230, 230, 230);
}

void draw_chess_grid(SDL_Renderer *r, int x0, int y0, int cols, int rows, int cell) {
    int col, row;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            int x = x0 + col * cell;
            int y = y0 + row * cell;
            int light = ((col + row) & 1) != 0;
            if (light) {
                fill_rect(r, x, y, cell, cell, UI_COL_CHESS_A_R, UI_COL_CHESS_A_G, UI_COL_CHESS_A_B);
            } else {
                fill_rect(r, x, y, cell, cell, UI_COL_CHESS_B_R, UI_COL_CHESS_B_G, UI_COL_CHESS_B_B);
            }
        }
    }
}

