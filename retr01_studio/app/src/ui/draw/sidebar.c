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

static void draw_worlds_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    R01World *w = r01_project_active_world(ui->project);
    int i, col, row;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    for (i = 0; i < R01_MAX_WORLDS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int y = lo->worlds_btns_y;
        int on = (i == ui->project->active_world);
        int hover = point_in_rect(lx, ly, x, y, UI_WORLD_BTN, UI_WORLD_BTN);

        if (on) {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i);
        font_draw(r, x + 1, y, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN);
        }
    }
    draw_chess_grid(r, UI_WORLDS_X, lo->worlds_grid_y, R01_GRID_MAX, R01_GRID_MAX, UI_WORLD_CELL);
    if (!w || !w->present) {
        return;
    }
    {
        int mark_idx = ui->play.active ? r01_play_screen_index(&ui->play, w) : w->default_screen;
        if (mark_idx < 0 || mark_idx >= w->screen_count || !w->screens[mark_idx].present) {
            mark_idx = r01_world_default_screen(w);
        }
        for (row = 0; row < R01_GRID_MAX; row++) {
            for (col = 0; col < R01_GRID_MAX; col++) {
                int idx = r01_world_screen_index(w, col, row);
                int x = UI_WORLDS_X + col * UI_WORLD_CELL;
                int y = lo->worlds_grid_y + row * UI_WORLD_CELL;
                int present = (idx >= 0 && w->screens[idx].present);
                int marked = present && idx == mark_idx;
                int hover = point_in_rect(lx, ly, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                if (present && !marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_PRESENT_R, UI_COL_PRESENT_G,
                              UI_COL_PRESENT_B);
                }
                if (marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_MARK_R, UI_COL_MARK_G, UI_COL_MARK_B);
                }
                if (hover) {
                    hover_overlay(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                }
            }
        }
        if (!ui->play.active) {
            int sel = ui->project->active_screen;
            if (sel >= 0 && sel < w->screen_count && w->screens[sel].present) {
                int x = UI_WORLDS_X + w->screens[sel].col * UI_WORLD_CELL;
                int y = lo->worlds_grid_y + w->screens[sel].row * UI_WORLD_CELL;
                draw_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, 255, 255, 255);
            }
        }
    }
}

static void draw_palettes(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = ui->pal_edit.open ? ui->pal_edit.row : (w ? w->default_pal_row : 0);
    int pal, c, i;
    int bg_strip_y = lo->pals_body_y;
    int spr_strip_y = lo->pals_body_y + UI_PAL_SWATCH;
    int row_btns_y = lo->pals_body_y + UI_PAL_SWATCH * 2;
    if (row < 0) {
        row = 0;
    }
    if (row >= R01_PAL_ROWS) {
        row = R01_PAL_ROWS - 1;
    }
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            r01_kit_rgb(ui->project->global_pal_bg[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, bg_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, spr_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
        }
    }
    for (i = 0; i < R01_PAL_ROWS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int on = (i == row);
        int hover = point_in_rect(ui->mouse_x, ui->mouse_y, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        if (on) {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i);
        font_draw_centered(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        }
    }
}

void draw_sidebar(UiState *ui, SDL_Renderer *r) {
    AccordionLayout lo;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    accordion_layout(ui, &lo);
    fill_rect(r, 0, 0, UI_SIDEBAR_W, UI_LOGIC_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);

    draw_accordion_header(r, lo.worlds_hdr_y, "Worlds", lo.worlds_open,
                          point_in_rect(lx, ly, 0, lo.worlds_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.worlds_open) {
        draw_worlds_body(ui, r, &lo);
    }

    draw_accordion_header(r, lo.pals_hdr_y, "Palettes", lo.pals_open,
                          point_in_rect(lx, ly, 0, lo.pals_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.pals_open) {
        draw_palettes(ui, r, &lo);
    }
}
