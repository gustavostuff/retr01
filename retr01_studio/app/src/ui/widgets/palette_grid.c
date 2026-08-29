#include "ui/widgets/widgets.h"
#include "ui/internal.h"

#include "retr01_studio/palette.h"

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
            int x = pal_x + c * UI_PAL_SWATCH;
            int y = pal_y + pal * UI_PAL_SWATCH;
            uint8_t idx;
            if (plane == UI_PAL_PLANE_BG) {
                idx = p->global_pal_bg[row][pal].idx[c];
            } else {
                idx = p->global_pal_spr[row][pal].idx[c];
            }
            r01_kit_rgb(idx, &cr, &cg, &cb);
            fill_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, cr, cg, cb);
            if (pal == sel_pal && c == sel_color) {
                draw_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, 255, 255, 255);
            }
        }
    }
}

int ui_palette_grid_hit(int lx, int ly, int pal_x, int pal_y, int *out_pal, int *out_color) {
    if (lx < pal_x || lx >= pal_x + R01_PALS_PER_ROW * UI_PAL_SWATCH || ly < pal_y ||
        ly >= pal_y + R01_PALS_PER_ROW * UI_PAL_SWATCH) {
        return 0;
    }
    if (out_pal) {
        *out_pal = (ly - pal_y) / UI_PAL_SWATCH;
    }
    if (out_color) {
        *out_color = (lx - pal_x) / UI_PAL_SWATCH;
    }
    return 1;
}
