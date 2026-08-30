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

static void draw_pal_plane_grid(SDL_Renderer *r, R01Project *p, int row, int plane, int x0, int y0,
                                int sel_plane, int sel_pal, int sel_color) {
    int pal, color;
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (color = 0; color < R01_PAL_COLORS; color++) {
            uint8_t cr, cg, cb;
            uint8_t master;
            int x = x0 + color * UI_PAL_EDIT_CELL;
            int y = y0 + pal * UI_PAL_EDIT_CELL;
            if (plane) {
                master = p->global_pal_spr[row][pal].idx[color];
            } else {
                master = p->global_pal_bg[row][pal].idx[color];
            }
            r01_kit_rgb(master, &cr, &cg, &cb);
            fill_rect(r, x, y, UI_PAL_EDIT_CELL, UI_PAL_EDIT_CELL, cr, cg, cb);
            if (plane == sel_plane && pal == sel_pal && color == sel_color) {
                draw_rect(r, x, y, UI_PAL_EDIT_CELL, UI_PAL_EDIT_CELL, 255, 255, 255);
            }
        }
    }
}

void draw_pal_modal(UiState *ui, SDL_Renderer *r) {
    PalModalLayout lo;
    int col, row;
    char row_label[24];
    int save_hover, cancel_hover;

    pal_modal_layout(ui, &lo);

    ui_modal_scrim(r, ui);
    ui_modal_panel(r, lo.mx, lo.my, UI_PAL_MODAL_W, UI_PAL_MODAL_H, "Global palettes");

    draw_label(r, lo.master_x, lo.my + UI_MODAL_BODY_Y, "Master 16x4");
    for (row = 0; row < UI_MASTER_ROWS; row++) {
        for (col = 0; col < UI_MASTER_COLS; col++) {
            uint8_t cr, cg, cb;
            int master = row * UI_MASTER_COLS + col;
            int x = lo.master_x + col * UI_MASTER_CELL;
            int y = lo.master_y + row * UI_MASTER_CELL;
            r01_kit_rgb(master, &cr, &cg, &cb);
            fill_rect(r, x, y, UI_MASTER_CELL - 1, UI_MASTER_CELL - 1, cr, cg, cb);
        }
    }

    snprintf(row_label, sizeof(row_label), "BG %d", ui->pal_edit.row);
    draw_label(r, lo.bg_x, lo.bg_label_y, row_label);
    draw_pal_plane_grid(r, ui->project, ui->pal_edit.row, 0, lo.bg_x, lo.bg_y, ui->pal_edit.plane,
                        ui->pal_edit.pal, ui->pal_edit.color);

    snprintf(row_label, sizeof(row_label), "SPR %d", ui->pal_edit.row);
    draw_label(r, lo.spr_x, lo.spr_label_y, row_label);
    draw_pal_plane_grid(r, ui->project, ui->pal_edit.row, 1, lo.spr_x, lo.spr_y, ui->pal_edit.plane,
                        ui->pal_edit.pal, ui->pal_edit.color);

    save_hover = point_in_rect(ui->mouse_x, ui->mouse_y, lo.master_x, lo.btn_y, lo.save_w, UI_BTN_H);
    cancel_hover =
        point_in_rect(ui->mouse_x, ui->mouse_y, lo.master_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    draw_button(r, lo.master_x, lo.btn_y, lo.save_w, "Save", 1, save_hover);
    draw_button(r, lo.master_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, "Cancel", 0, cancel_hover);

    {
        uint8_t *slot = pal_edit_slot_ptr(ui->project, &ui->pal_edit);
        if (slot) {
            int master = *slot & 63;
            int scol = master % UI_MASTER_COLS;
            int srow = master / UI_MASTER_COLS;
            int x = lo.master_x + scol * UI_MASTER_CELL;
            int y = lo.master_y + srow * UI_MASTER_CELL;
            draw_rect(r, x, y, UI_MASTER_CELL - 1, UI_MASTER_CELL - 1, 255, 255, 255);
        }
    }
}

