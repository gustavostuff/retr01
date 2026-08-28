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

static uint8_t *pal_edit_slot_ptr(R01Project *p, const UiPalEdit *pe) {
    if (!p || !pe || pe->row < 0 || pe->row >= R01_PAL_ROWS || pe->pal < 0 || pe->pal >= R01_PALS_PER_ROW ||
        pe->color < 0 || pe->color >= R01_PAL_COLORS) {
        return NULL;
    }
    if (pe->plane) {
        return &p->global_pal_spr[pe->row][pe->pal].idx[pe->color];
    }
    return &p->global_pal_bg[pe->row][pe->pal].idx[pe->color];
}

static void pal_edit_set_master(UiState *ui, int master) {
    uint8_t *slot;
    if (master < 0) {
        master = 0;
    }
    if (master >= R01_MASTER_COLORS) {
        master = R01_MASTER_COLORS - 1;
    }
    slot = pal_edit_slot_ptr(ui->project, &ui->pal_edit);
    if (slot) {
        *slot = (uint8_t)master;
    }
}

void pal_edit_nudge_master(UiState *ui, int wheel_y, int shift) {
    uint8_t *slot;
    int master, row, col, step;
    if (!wheel_y) {
        return;
    }
    slot = pal_edit_slot_ptr(ui->project, &ui->pal_edit);
    if (!slot) {
        return;
    }
    step = wheel_y > 0 ? 1 : -1;
    master = *slot & 63;
    row = master / UI_MASTER_COLS;
    col = master % UI_MASTER_COLS;
    if (shift) {
        col += step;
        if (col < 0) {
            col = 0;
        }
        if (col >= UI_MASTER_COLS) {
            col = UI_MASTER_COLS - 1;
        }
    } else {
        row += step;
        if (row < 0) {
            row = 0;
        }
        if (row >= UI_MASTER_ROWS) {
            row = UI_MASTER_ROWS - 1;
        }
    }
    *slot = (uint8_t)(row * UI_MASTER_COLS + col);
}

int palette_strip_hit(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int y0;
    int row_btns_y;
    accordion_layout(ui, &lo);
    if (!lo.pals_open) {
        return 0;
    }
    y0 = lo.pals_body_y;
    row_btns_y = y0 + UI_PAL_SWATCH * 2;
    if (lx < 0 || lx >= UI_SIDEBAR_W || ly < y0 || ly >= row_btns_y + UI_BTN_H) {
        return 0;
    }
    if (ly >= row_btns_y && ly < row_btns_y + UI_BTN_H) {
        return 1;
    }
    return ly < row_btns_y;
}

int palette_row_btn_hit(const UiState *ui, int lx, int ly, int *out_row) {
    AccordionLayout lo;
    int row_btns_y;
    int i;
    accordion_layout(ui, &lo);
    if (!lo.pals_open) {
        return 0;
    }
    row_btns_y = lo.pals_body_y + UI_PAL_SWATCH * 2;
    if (lx < 0 || lx >= UI_SIDEBAR_W || ly < row_btns_y || ly >= row_btns_y + UI_BTN_H) {
        return 0;
    }
    for (i = 0; i < R01_PAL_ROWS; i++) {
        int x = i * UI_WORLD_BTN;
        if (lx >= x && lx < x + UI_WORLD_BTN) {
            if (out_row) {
                *out_row = i;
            }
            return 1;
        }
    }
    return 0;
}

static void pal_edit_snapshot(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    if (!ui || !ui->project) {
        return;
    }
    memcpy(ui->pal_edit.snap_bg, ui->project->global_pal_bg, sizeof(ui->pal_edit.snap_bg));
    memcpy(ui->pal_edit.snap_spr, ui->project->global_pal_spr, sizeof(ui->pal_edit.snap_spr));
    ui->pal_edit.snap_default_row = w ? w->default_pal_row : 0;
    ui->pal_edit.snap_valid = 1;
}

static void pal_edit_restore(UiState *ui) {
    R01World *w;
    if (!ui || !ui->project || !ui->pal_edit.snap_valid) {
        return;
    }
    memcpy(ui->project->global_pal_bg, ui->pal_edit.snap_bg, sizeof(ui->pal_edit.snap_bg));
    memcpy(ui->project->global_pal_spr, ui->pal_edit.snap_spr, sizeof(ui->pal_edit.snap_spr));
    w = r01_project_active_world(ui->project);
    if (w) {
        w->default_pal_row = ui->pal_edit.snap_default_row;
    }
    ui->pal_edit.snap_valid = 0;
}

void pal_edit_close(UiState *ui) {
    ui->pal_edit.open = 0;
    ui->pal_edit.snap_valid = 0;
}

void pal_edit_cancel(UiState *ui) {
    pal_edit_restore(ui);
    pal_edit_close(ui);
}

void pal_edit_save(UiState *ui) {
    pal_edit_close(ui);
    ui_toast(ui, "palettes saved", 0);
}

void pal_edit_open(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    int row = w ? w->default_pal_row : 0;
    if (row < 0) {
        row = 0;
    }
    if (row >= R01_PAL_ROWS) {
        row = R01_PAL_ROWS - 1;
    }
    pal_edit_snapshot(ui);
    ui->pal_edit.open = 1;
    ui->pal_edit.row = row;
    ui->pal_edit.plane = 0;
    ui->pal_edit.pal = 0;
    ui->pal_edit.color = 1;
    ui->menu.open = 0;
}

int pal_modal_master_hit(int lx, int ly, int *out_col, int *out_row) {
    PalModalLayout lo;
    int col, row;
    pal_modal_layout(&lo);
    if (lx < lo.master_x || ly < lo.master_y ||
        lx >= lo.master_x + UI_MASTER_COLS * UI_MASTER_CELL ||
        ly >= lo.master_y + UI_MASTER_ROWS * UI_MASTER_CELL) {
        return 0;
    }
    col = (lx - lo.master_x) / UI_MASTER_CELL;
    row = (ly - lo.master_y) / UI_MASTER_CELL;
    if (out_col) {
        *out_col = col;
    }
    if (out_row) {
        *out_row = row;
    }
    return 1;
}

int pal_modal_plane_hit(int lx, int ly, int plane, int *out_pal, int *out_color) {
    PalModalLayout lo;
    int x0, y0, pal, color;
    pal_modal_layout(&lo);
    x0 = plane ? lo.spr_x : lo.bg_x;
    y0 = plane ? lo.spr_y : lo.bg_y;
    if (lx < x0 || ly < y0 || lx >= x0 + R01_PALS_PER_ROW * UI_PAL_EDIT_CELL ||
        ly >= y0 + R01_PALS_PER_ROW * UI_PAL_EDIT_CELL) {
        return 0;
    }
    color = (lx - x0) / UI_PAL_EDIT_CELL;
    pal = (ly - y0) / UI_PAL_EDIT_CELL;
    if (out_pal) {
        *out_pal = pal;
    }
    if (out_color) {
        *out_color = color;
    }
    return 1;
}

void pal_edit_set_row(UiState *ui, int row, int commit_default) {
    R01World *w;
    if (row < 0 || row >= R01_PAL_ROWS) {
        return;
    }
    if (ui->pal_edit.open) {
        ui->pal_edit.row = row;
        return;
    }
    ui->pal_edit.row = row;
    if (commit_default) {
        w = r01_project_active_world(ui->project);
        if (w) {
            w->default_pal_row = row;
        }
    }
}

int pal_modal_handle(UiState *ui, int lx, int ly, int down) {
    PalModalLayout lo;
    int col, row, pal, color;
    if (!down) {
        return 1;
    }
    pal_modal_layout(&lo);
    if (lx >= lo.master_x && lx < lo.master_x + lo.save_w && ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        pal_edit_save(ui);
        return 1;
    }
    if (lx >= lo.master_x + lo.save_w + UI_UNIT && lx < lo.master_x + lo.save_w + UI_UNIT + lo.cancel_w &&
        ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        pal_edit_cancel(ui);
        return 1;
    }
    if (pal_modal_master_hit(lx, ly, &col, &row)) {
        pal_edit_set_master(ui, row * UI_MASTER_COLS + col);
        return 1;
    }
    if (pal_modal_plane_hit(lx, ly, 0, &pal, &color)) {
        ui->pal_edit.plane = 0;
        ui->pal_edit.pal = pal;
        ui->pal_edit.color = color;
        return 1;
    }
    if (pal_modal_plane_hit(lx, ly, 1, &pal, &color)) {
        ui->pal_edit.plane = 1;
        ui->pal_edit.pal = pal;
        ui->pal_edit.color = color;
        return 1;
    }
    return 1;
}

