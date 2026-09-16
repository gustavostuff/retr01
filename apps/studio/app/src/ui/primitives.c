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
uint8_t *g_bg0_btn_rgba;
int g_bg0_btn_w;
int g_bg0_btn_h;
uint8_t *g_bg1_btn_rgba;
int g_bg1_btn_w;
int g_bg1_btn_h;
uint8_t *g_bg_bank_btn_rgba;
int g_bg_bank_btn_w;
int g_bg_bank_btn_h;
uint8_t *g_spr_bank_btn_rgba;
int g_spr_bank_btn_w;
int g_spr_bank_btn_h;

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
void ui_toast(UiState *ui, const char *msg, int is_error) {
    if (!ui || !msg) {
        return;
    }
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_error = is_error;
    ui->toast_until = SDL_GetTicks() + UI_TOAST_MS;
}

void ui_tooltip_set(UiState *ui, int x, int y, const char *line1, const char *line2) {
    /* Immediate show (legacy). Prefer ui_tooltip_hover for delayed cursor tooltips. */
    if (!ui) {
        return;
    }
    ui->tooltip_active = 1;
    ui->tooltip_hit = 1;
    ui->tooltip_x = x;
    ui->tooltip_y = y;
    ui->tooltip_since_ms = 0;
    if (line2 && line2[0]) {
        snprintf(ui->tooltip, sizeof(ui->tooltip), "%s\n%s", line1 ? line1 : "", line2);
        snprintf(ui->tooltip_key, sizeof(ui->tooltip_key), "%s\n%s", line1 ? line1 : "", line2);
    } else {
        snprintf(ui->tooltip, sizeof(ui->tooltip), "%s", line1 ? line1 : "");
        snprintf(ui->tooltip_key, sizeof(ui->tooltip_key), "%s", line1 ? line1 : "");
    }
}

void ui_tooltip_hover(UiState *ui, int x, int y, const char *line1, const char *line2) {
    char key[160];
    if (!ui) {
        return;
    }
    if (line2 && line2[0]) {
        snprintf(key, sizeof(key), "%s\n%s", line1 ? line1 : "", line2);
    } else {
        snprintf(key, sizeof(key), "%s", line1 ? line1 : "");
    }
    /* Any cursor move (or target change) hides immediately and restarts the delay. */
    if (strcmp(ui->tooltip_key, key) != 0 || ui->tooltip_arm_x != x || ui->tooltip_arm_y != y) {
        snprintf(ui->tooltip_key, sizeof(ui->tooltip_key), "%s", key);
        snprintf(ui->tooltip, sizeof(ui->tooltip), "%s", key);
        ui->tooltip_arm_x = x;
        ui->tooltip_arm_y = y;
        ui->tooltip_since_ms = SDL_GetTicks();
        ui->tooltip_active = 0;
    }
    ui->tooltip_x = x;
    ui->tooltip_y = y;
    ui->tooltip_hit = 1;
}

void ui_tooltip_frame_begin(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->tooltip_hit = 0;
}

void ui_tooltip_frame_end(UiState *ui) {
    if (!ui) {
        return;
    }
    if (!ui->tooltip_hit) {
        ui->tooltip_active = 0;
        ui->tooltip[0] = '\0';
        ui->tooltip_key[0] = '\0';
        ui->tooltip_since_ms = 0;
        ui->tooltip_arm_x = 0;
        ui->tooltip_arm_y = 0;
        return;
    }
    if (!ui->tooltip_active && ui->tooltip_since_ms != 0 &&
        SDL_GetTicks() - ui->tooltip_since_ms >= UI_TOOLTIP_DELAY_MS) {
        ui->tooltip_active = 1;
    }
}

void ui_tooltip_clear(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->tooltip_active = 0;
    ui->tooltip_hit = 0;
    ui->tooltip[0] = '\0';
    ui->tooltip_key[0] = '\0';
    ui->tooltip_since_ms = 0;
    ui->tooltip_arm_x = 0;
    ui->tooltip_arm_y = 0;
}

void ui_focus_set(UiState *ui, int focus) {
    if (!ui) {
        return;
    }
    ui->focus = focus;
}

int ui_focus_get(const UiState *ui) {
    return ui ? ui->focus : UI_FOCUS_NONE;
}

void ui_focus_clear(UiState *ui) {
    ui_focus_set(ui, UI_FOCUS_NONE);
}

void draw_tooltip(UiState *ui, SDL_Renderer *r) {
    int tw, th, x, y;
    int pad = UI_UNIT;
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
    tw = snap8(tw + pad * 2);
    th = line2[0] ? (UI_BTN_H * 2) : UI_BTN_H;
    /* Prefer lower-right of cursor. Flip to keep the full tip on-screen. */
    x = ui->tooltip_x + pad;
    y = ui->tooltip_y + pad;
    if (x + tw > ui_logic_w(ui) - pad) {
        x = ui->tooltip_x - tw - pad;
    }
    if (y + th > ui_logic_h(ui) - pad) {
        y = ui->tooltip_y - th - pad;
    }
    if (x < pad) {
        x = pad;
    }
    if (y < pad) {
        y = pad;
    }
    if (x + tw > ui_logic_w(ui) - pad) {
        x = ui_logic_w(ui) - pad - tw;
    }
    if (y + th > ui_logic_h(ui) - pad) {
        y = ui_logic_h(ui) - pad - th;
    }
    /* Soft yellow BG, black FG. */
    fill_rect(r, x, y, tw, th, 250, 230, 140);
    draw_rect(r, x, y, tw, th, 40, 40, 40);
    font_draw(r, x + pad, y + 4, line1, 0, 0, 0);
    if (line2[0]) {
        font_draw(r, x + pad, y + UI_BTN_H + 4, line2, 0, 0, 0);
    }
}

#define UI_ANTS_PERIOD 4
#define UI_ANTS_MS 80

void draw_marching_ants_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 alpha) {
    int phase;
    int i;
    int len;
    if (!r || w < 2 || h < 2) {
        return;
    }
    phase = (int)((SDL_GetTicks() / UI_ANTS_MS) % UI_ANTS_PERIOD);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    /* Pattern: black, transparent, white, transparent (1px each). */
    len = w;
    for (i = 0; i < len; i++) {
        int step = (i + phase) % UI_ANTS_PERIOD;
        if (step == 0) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);
            SDL_RenderDrawPoint(r, x + i, y);
            SDL_RenderDrawPoint(r, x + i, y + h - 1);
        } else if (step == 2) {
            SDL_SetRenderDrawColor(r, 255, 255, 255, alpha);
            SDL_RenderDrawPoint(r, x + i, y);
            SDL_RenderDrawPoint(r, x + i, y + h - 1);
        }
    }
    len = h - 2;
    for (i = 0; i < len; i++) {
        int step = (i + 1 + phase) % UI_ANTS_PERIOD;
        if (step == 0) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);
            SDL_RenderDrawPoint(r, x, y + 1 + i);
            SDL_RenderDrawPoint(r, x + w - 1, y + 1 + i);
        } else if (step == 2) {
            SDL_SetRenderDrawColor(r, 255, 255, 255, alpha);
            SDL_RenderDrawPoint(r, x, y + 1 + i);
            SDL_RenderDrawPoint(r, x + w - 1, y + 1 + i);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void draw_marching_ants(SDL_Renderer *r, int x, int y, int w, int h) {
    draw_marching_ants_a(r, x, y, w, h, 255);
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

void draw_brush_preview(SDL_Renderer *r, const R01Project *p, int row, int pal, int color, int mx, int my) {
    (void)r;
    (void)p;
    (void)row;
    (void)pal;
    (void)color;
    (void)mx;
    (void)my;
    /* Deprecated: callers draw a canvas-aligned paint ghost instead. */
}

void draw_paint_pixel_preview(SDL_Renderer *r, const R01Project *p, int row, UiPalPlane plane, int pal, int color,
                              int px, int py, int cell) {
    uint8_t cr, cg, cb;
    uint8_t idx;
    if (!p || !r || cell < 1) {
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
    if (plane == UI_PAL_PLANE_SPR) {
        idx = p->global_pal_spr[row][pal].idx[color & 3u];
    } else {
        idx = p->global_pal_bg[row][pal].idx[color & 3u];
    }
    r01_kit_rgb(idx, &cr, &cg, &cb);
    fill_rect(r, px, py, cell, cell, cr, cg, cb);
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

