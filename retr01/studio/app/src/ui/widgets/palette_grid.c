#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/palette.h"

#include <stdio.h>

static uint8_t *palette_slot_ptr(R01Project *p, int row, int pal, int color, UiPalPlane plane) {
    if (!p || row < 0 || row >= R01_PAL_ROWS || pal < 0 || pal >= R01_PALS_PER_ROW || color < 0 ||
        color >= R01_PAL_COLORS) {
        return NULL;
    }
    if (plane == UI_PAL_PLANE_SPR) {
        return &p->global_pal_spr[row][pal].idx[color];
    }
    return &p->global_pal_bg[row][pal].idx[color];
}

static void label_contrast(uint8_t cr, uint8_t cg, uint8_t cb, Uint8 *out_r, Uint8 *out_g, Uint8 *out_b) {
    int y = (299 * (int)cr + 587 * (int)cg + 114 * (int)cb) / 1000;
    if (y >= 128) {
        *out_r = *out_g = *out_b = 0;
    } else {
        *out_r = *out_g = *out_b = 255;
    }
}

void ui_palette_grid_draw(SDL_Renderer *r, const R01Project *p, int row, int pal_x, int pal_y, int sel_pal,
                          int sel_color, UiPalPlane plane) {
    int pal, c;
    if (!p || !r) {
        return;
    }
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            uint8_t lr, lg, lb;
            int x = pal_x + c * UI_PAL_GRID_CELL;
            int y = pal_y + pal * UI_PAL_GRID_CELL;
            int tw;
            uint8_t idx;
            char code[8];
            if (plane == UI_PAL_PLANE_BG) {
                idx = p->global_pal_bg[row][pal].idx[c];
            } else {
                idx = p->global_pal_spr[row][pal].idx[c];
            }
            idx = (uint8_t)(idx & 63u);
            r01_kit_rgb(idx, &cr, &cg, &cb);
            fill_rect(r, x, y, UI_PAL_GRID_CELL, UI_PAL_GRID_CELL, cr, cg, cb);
            snprintf(code, sizeof(code), "%02X", (unsigned)idx);
            label_contrast(cr, cg, cb, &lr, &lg, &lb);
            tw = font_text_width(code);
            font_draw_sized_alpha(r, x + (UI_PAL_GRID_CELL - tw) / 2, y + (UI_PAL_GRID_CELL - font_line_h()) / 2,
                                  R01_UI_FONT_PX, code, lr, lg, lb, 128);
            if (pal == sel_pal && c == sel_color) {
                draw_rect(r, x, y, UI_PAL_GRID_CELL, UI_PAL_GRID_CELL, 255, 255, 255);
            }
        }
    }
}

int ui_palette_grid_hit(int lx, int ly, int pal_x, int pal_y, int *out_pal, int *out_color) {
    if (lx < pal_x || lx >= pal_x + R01_PALS_PER_ROW * UI_PAL_GRID_CELL || ly < pal_y ||
        ly >= pal_y + R01_PALS_PER_ROW * UI_PAL_GRID_CELL) {
        return 0;
    }
    if (out_pal) {
        *out_pal = (ly - pal_y) / UI_PAL_GRID_CELL;
    }
    if (out_color) {
        *out_color = (lx - pal_x) / UI_PAL_GRID_CELL;
    }
    return 1;
}

void ui_palette_grid_nudge(R01Project *p, int row, UiPalPlane plane, int pal, int color, int wheel_y,
                           int shift) {
    uint8_t *slot;
    int master, mrow, mcol, step;
    if (!wheel_y) {
        return;
    }
    slot = palette_slot_ptr(p, row, pal, color, plane);
    if (!slot) {
        return;
    }
    step = wheel_y < 0 ? 1 : -1;
    master = *slot & 63;
    mrow = master / UI_MASTER_COLS;
    mcol = master % UI_MASTER_COLS;
    if (shift) {
        mcol += step;
        if (mcol < 0) {
            mcol = 0;
        }
        if (mcol >= UI_MASTER_COLS) {
            mcol = UI_MASTER_COLS - 1;
        }
    } else {
        mrow += step;
        if (mrow < 0) {
            mrow = 0;
        }
        if (mrow >= UI_MASTER_ROWS) {
            mrow = UI_MASTER_ROWS - 1;
        }
    }
    *slot = (uint8_t)(mrow * UI_MASTER_COLS + mcol);
}
