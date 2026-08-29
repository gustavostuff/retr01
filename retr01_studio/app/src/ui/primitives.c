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

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
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

