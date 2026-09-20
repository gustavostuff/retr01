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
    step = wheel_y < 0 ? 1 : -1;
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
    if (lo.pals_body_h < 1) {
        return 0;
    }
    y0 = lo.pals_body_y;
    row_btns_y = y0 + UI_PAL_SWATCH * 2;
    if (lx < 0 || lx >= UI_SIDEBAR_W || ly < y0 || ly >= y0 + lo.pals_body_h) {
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
    if (lo.pals_body_h < UI_BTN_H) {
        return 0;
    }
    row_btns_y = lo.pals_body_y + UI_PAL_SWATCH * 2;
    if (lx < 0 || lx >= UI_SIDEBAR_W || ly < row_btns_y || ly >= lo.pals_body_y + lo.pals_body_h) {
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
    R01Project *p = ui->project;
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
    R01Project *p = ui->project;
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
    R01Project *p = ui->project;
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

int pal_modal_master_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row) {
    PalModalLayout lo;
    int col, row;
    pal_modal_layout(ui, &lo);
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

int pal_modal_plane_hit(const UiState *ui, int lx, int ly, int plane, int *out_pal, int *out_color) {
    PalModalLayout lo;
    int x0, y0, pal, color;
    pal_modal_layout(ui, &lo);
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
        R01Project *p = ui->project;
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
    pal_modal_layout(ui, &lo);
    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        pal_edit_cancel(ui);
        return 1;
    }
    if (lx >= lo.left_btn_x && lx < lo.left_btn_x + lo.save_w && ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        pal_edit_save(ui);
        return 1;
    }
    if (lx >= lo.left_btn_x + lo.save_w + UI_UNIT && lx < lo.left_btn_x + lo.save_w + UI_UNIT + lo.cancel_w &&
        ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        pal_edit_cancel(ui);
        return 1;
    }
    if (pal_modal_master_hit(ui, lx, ly, &col, &row)) {
        pal_edit_set_master(ui, row * UI_MASTER_COLS + col);
        return 1;
    }
    if (pal_modal_plane_hit(ui, lx, ly, 0, &pal, &color)) {
        ui->pal_edit.plane = 0;
        ui->pal_edit.pal = pal;
        ui->pal_edit.color = color;
        return 1;
    }
    if (pal_modal_plane_hit(ui, lx, ly, 1, &pal, &color)) {
        ui->pal_edit.plane = 1;
        ui->pal_edit.pal = pal;
        ui->pal_edit.color = color;
        return 1;
    }
    return 1;
}

void pal_modal_layout(const UiState *ui, PalModalLayout *lo) {
    enum { C_MASTER_LAB = 1, C_MASTER, C_BG_LAB, C_BG, C_SPR_LAB, C_SPR, C_FOOTER };
    static const UiPanelCell cells[] = {
        {C_MASTER_LAB, 0, 0, 3, 1},
        {C_MASTER, 0, 1, 3, 1},
        {C_BG_LAB, 0, 3, 1, 1},
        {C_BG, 0, 4, 1, 1},
        {C_SPR_LAB, 2, 3, 1, 1},
        {C_SPR, 2, 4, 1, 1},
        {C_FOOTER, 0, 6, 3, 1},
    };
    static const int row_hs[] = {
        UI_BTN_H,
        UI_MASTER_ROWS * UI_MASTER_CELL,
        UI_UNIT,
        UI_BTN_H,
        R01_PALS_PER_ROW * UI_PAL_EDIT_CELL,
        UI_UNIT,
        UI_BTN_H,
    };
    UiPanel panel;
    int pad = UI_UNIT;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int plane = R01_PALS_PER_ROW * UI_PAL_EDIT_CELL;
    int gap = UI_UNIT * 2;
    int i;

    ui_panel_init(&panel, 3, 7, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, (int)(sizeof(cells) / sizeof(cells[0])));
    ui_panel_set_col_w(&panel, 0, plane);
    ui_panel_set_col_w(&panel, 1, gap);
    ui_panel_set_col_w(&panel, 2, plane);
    for (i = 0; i < (int)(sizeof(row_hs) / sizeof(row_hs[0])); i++) {
        ui_panel_set_row_h(&panel, i, row_hs[i]);
    }
    ui_panel_layout(&panel, 0, 0);

    lo->mw = pad + panel.total_w + pad;
    lo->mh = UI_BTN_H + pad + panel.total_h + pad;
    lo->mx = (ui_logic_w(ui) - lo->mw) / 2;
    lo->my = (ui_logic_h(ui) - lo->mh) / 2;
    content_x = lo->mx + pad;
    content_y = lo->my + UI_BTN_H + pad;
    ui_panel_layout(&panel, content_x, content_y);

    ui_panel_cell(&panel, C_MASTER, &cx, &cy, &cw, &ch);
    lo->master_x = cx;
    lo->master_y = cy;
    ui_panel_cell(&panel, C_BG_LAB, &cx, &cy, &cw, &ch);
    lo->bg_label_y = cy;
    ui_panel_cell(&panel, C_BG, &cx, &cy, &cw, &ch);
    lo->bg_x = cx;
    lo->bg_y = cy;
    ui_panel_cell(&panel, C_SPR_LAB, &cx, &cy, &cw, &ch);
    lo->spr_label_y = cy;
    ui_panel_cell(&panel, C_SPR, &cx, &cy, &cw, &ch);
    lo->spr_x = cx;
    lo->spr_y = cy;
    ui_panel_cell(&panel, C_FOOTER, &cx, &cy, &cw, &ch);
    lo->btn_y = cy;
    lo->left_btn_x = content_x;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
#if UI_PANEL_DEBUG_GRID
    lo->dbg_panel = panel;
#endif
}

