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

int ui_mode_panel_w(void) {
    int label_x = UI_MODE_RADIO + UI_MODE_GAP;
    int w0 = label_x + label_width("Tile selection");
    int w1 = label_x + label_width("Tile paint");
    return w0 > w1 ? w0 : w1;
}

void ui_editor_layout(const UiState *ui, int *screen_x, int *screen_y, int *mode_x, int *mode_y0) {
    int play_active = ui && ui->play.active;
    int sx = UI_SIDEBAR_W + (UI_MAIN_W - UI_SCREEN_W) / 2;
    int sy = (UI_LOGIC_H - UI_SCREEN_H) / 2;
    int mx = sx + UI_SCREEN_W + UI_UNIT;
    int my0 = sy;
    int panel_w;
    if (play_active) {
        if (screen_x) {
            *screen_x = sx;
        }
        if (screen_y) {
            *screen_y = sy;
        }
        return;
    }
    panel_w = ui_mode_panel_w();
    mx = sx + UI_SCREEN_W + UI_UNIT;
    if (mx + panel_w > UI_LOGIC_W - UI_UNIT) {
        sx = UI_LOGIC_W - UI_UNIT - panel_w - UI_UNIT - UI_SCREEN_W;
        if (sx < UI_SIDEBAR_W + UI_UNIT) {
            sx = UI_SIDEBAR_W + UI_UNIT;
        }
        mx = sx + UI_SCREEN_W + UI_UNIT;
    }
    if (screen_x) {
        *screen_x = sx;
    }
    if (screen_y) {
        *screen_y = sy;
    }
    if (mode_x) {
        *mode_x = mx;
    }
    if (mode_y0) {
        *mode_y0 = my0;
    }
}

int ui_mode_label_x(int mode_x) {
    return mode_x + UI_MODE_RADIO + UI_MODE_GAP;
}

int screen_mode_row_hit(const UiState *ui, int lx, int ly, int row) {
    int sx, sy, mx, my0;
    int y;
    if (!ui || ui->play.active) {
        return 0;
    }
    ui_editor_layout(ui, &sx, &sy, &mx, &my0);
    y = my0 + row * UI_MODE_ROW_H;
    return point_in_rect(lx, ly, mx, y, ui_mode_panel_w(), UI_MODE_ROW_H);
}

int screen_mode_hit(const UiState *ui, int lx, int ly, int *out_row) {
    if (screen_mode_row_hit(ui, lx, ly, 0)) {
        if (out_row) {
            *out_row = UI_SCREEN_MODE_SEL;
        }
        return 1;
    }
    if (screen_mode_row_hit(ui, lx, ly, 1)) {
        if (out_row) {
            *out_row = UI_SCREEN_MODE_PAINT;
        }
        return 1;
    }
    return 0;
}

int play_btn_w(const UiState *ui) {
    return label_width(ui->play.active ? "Stop" : "Play");
}

int play_btn_x(const UiState *ui) {
    int sx, sy, mx, my0;
    ui_editor_layout(ui, &sx, &sy, &mx, &my0);
    return sx + (UI_SCREEN_W - play_btn_w(ui)) / 2;
}

int play_btn_y(void) {
    return (UI_LOGIC_H - UI_SCREEN_H) / 2 - UI_UNIT - UI_BTN_H;
}

int play_button_hit(const UiState *ui, int lx, int ly) {
    int x = play_btn_x(ui);
    int y = play_btn_y();
    int w = play_btn_w(ui);
    return lx >= x && lx < x + w && ly >= y && ly < y + UI_BTN_H;
}

void screen_origin(const UiState *ui, int *ox, int *oy) {
    int sx, sy, mx, my0;
    ui_editor_layout(ui, &sx, &sy, &mx, &my0);
    *ox = sx;
    *oy = sy;
}

int screen_hit(const UiState *ui, int lx, int ly, int *out_tx, int *out_ty) {
    int ox, oy;
    int lx0, ly0;
    screen_origin(ui, &ox, &oy);
    if (lx < ox || ly < oy || lx >= ox + UI_SCREEN_W || ly >= oy + UI_SCREEN_H) {
        return 0;
    }
    lx0 = (lx - ox) / UI_SCREEN_SCALE;
    ly0 = (ly - oy) / UI_SCREEN_SCALE;
    if (lx0 >= R01_SCREEN_PX_W || ly0 >= R01_SCREEN_PX_H) {
        return 0;
    }
    if (out_tx) {
        *out_tx = lx0 / 8;
    }
    if (out_ty) {
        *out_ty = ly0 / 8;
    }
    return 1;
}

void accordion_layout(const UiState *ui, AccordionLayout *lo) {
    int y = 0;
    lo->worlds_hdr_y = y;
    y += UI_BTN_H;
    lo->worlds_open = (ui->accordion_open == UI_ACC_WORLDS);
    if (lo->worlds_open) {
        lo->worlds_btns_y = y;
        lo->worlds_grid_y = y + UI_WORLD_BTN;
        y += UI_WORLDS_BODY_H;
    } else {
        lo->worlds_btns_y = -1;
        lo->worlds_grid_y = -1;
    }
    lo->pals_hdr_y = y;
    y += UI_BTN_H;
    lo->pals_open = (ui->accordion_open == UI_ACC_PALS);
    if (lo->pals_open) {
        lo->pals_body_y = y;
    } else {
        lo->pals_body_y = -1;
    }
}
int world_cell_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row) {
    AccordionLayout lo;
    int x0 = UI_WORLDS_X;
    int y0;
    int col, row;
    accordion_layout(ui, &lo);
    if (!lo.worlds_open) {
        return 0;
    }
    y0 = lo.worlds_grid_y;
    if (lx < x0 || ly < y0 || lx >= x0 + UI_WORLD_VIEW || ly >= y0 + UI_WORLD_VIEW) {
        return 0;
    }
    col = (lx - x0) / UI_WORLD_CELL;
    row = (ly - y0) / UI_WORLD_CELL;
    if (col < 0 || row < 0 || col >= R01_GRID_MAX || row >= R01_GRID_MAX) {
        return 0;
    }
    if (out_col) {
        *out_col = col;
    }
    if (out_row) {
        *out_row = row;
    }
    return 1;
}

int world_btn_hit(const UiState *ui, int lx, int ly, int *out_wi) {
    AccordionLayout lo;
    int i;
    int y;
    accordion_layout(ui, &lo);
    if (!lo.worlds_open) {
        return 0;
    }
    y = lo.worlds_btns_y;
    if (ly < y || ly >= y + UI_WORLD_BTN) {
        return 0;
    }
    for (i = 0; i < R01_MAX_WORLDS; i++) {
        int x;
        ui_world_btn_pos(i, y, &x, NULL);
        if (lx >= x && lx < x + UI_WORLD_BTN) {
            if (out_wi) {
                *out_wi = i;
            }
            return 1;
        }
    }
    return 0;
}

int accordion_header_hit(const UiState *ui, int lx, int ly, int *out_section) {
    AccordionLayout lo;
    if (lx < 0 || lx >= UI_SIDEBAR_W) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (ly >= lo.worlds_hdr_y && ly < lo.worlds_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_WORLDS;
        }
        return 1;
    }
    if (ly >= lo.pals_hdr_y && ly < lo.pals_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_PALS;
        }
        return 1;
    }
    return 0;
}

void accordion_toggle(UiState *ui, int section) {
    if (ui->accordion_open == section) {
        ui->accordion_open = UI_ACC_NONE;
    } else {
        ui->accordion_open = section;
    }
}

void draw_accordion_header(SDL_Renderer *r, int y, const char *title, int open, int hover) {
    if (open) {
        fill_rect(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    } else {
        fill_rect(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
    }
    font_draw_centered(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, title, 230, 230, 230);
    if (hover) {
        hover_overlay(r, 0, y, UI_SIDEBAR_W, UI_BTN_H);
    }
}
static int modal_x(void) {
    return (UI_LOGIC_W - UI_MODAL_W) / 2;
}

static int modal_y(void) {
    return (UI_LOGIC_H - UI_MODAL_H) / 2;
}

void tile_modal_layout(TileModalLayout *lo) {
    lo->mx = modal_x();
    lo->my = modal_y();
    lo->pal_x = lo->mx + UI_UNIT * 2;
    lo->pal_label_y = lo->my + UI_MODAL_BODY_Y;
    lo->pal_y = lo->pal_label_y + UI_BTN_H;
    lo->canvas_x = lo->mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
    lo->canvas_y = lo->my + UI_MODAL_BODY_Y;
    lo->btn_y = lo->my + UI_MODAL_H - UI_BTN_H - UI_UNIT;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}

static int pal_modal_x(void) {
    return (UI_LOGIC_W - UI_PAL_MODAL_W) / 2;
}

static int pal_modal_y(void) {
    return (UI_LOGIC_H - UI_PAL_MODAL_H) / 2;
}

void pal_modal_layout(PalModalLayout *lo) {
    lo->mx = pal_modal_x();
    lo->my = pal_modal_y();
    lo->master_x = lo->mx + UI_UNIT * 2;
    lo->master_y = lo->my + UI_MODAL_BODY_Y + UI_BTN_H;
    lo->bg_label_y = lo->master_y + UI_MASTER_ROWS * UI_MASTER_CELL + UI_UNIT;
    lo->bg_x = lo->master_x;
    lo->bg_y = lo->bg_label_y + UI_BTN_H;
    lo->spr_label_y = lo->bg_label_y;
    lo->spr_x = lo->bg_x + R01_PALS_PER_ROW * UI_PAL_EDIT_CELL + UI_UNIT * 2;
    lo->spr_y = lo->bg_y;
    lo->btn_y = lo->my + UI_PAL_MODAL_H - UI_BTN_H - UI_UNIT;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}
