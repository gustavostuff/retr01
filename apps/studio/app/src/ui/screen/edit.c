#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/undo/undo_cmds.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/collision.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void screen_refresh_tile(const R01Project *p, R01Screen *s) {
    if (p && s && s->present) {
        r01_screen_fill_pixels_from_bank(p, s);
    }
}

void screen_refresh_sel(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    R01Screen *s = ui_edit_map_screen(ui);
    screen_refresh_tile(p, s);
}

void screen_set_sel_bank(UiState *ui, int bank) {
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    R01Screen *s = ui_edit_map_screen(ui);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old = s->attrs[cell];
            s->attrs[cell] =
                r01_attr_merge(old, bank, r01_attr_pal(old), r01_attr_flip_h(old), r01_attr_flip_v(old));
        }
    }
    screen_refresh_sel(ui);
}

void screen_set_sel_pal(UiState *ui, int pal) {
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    R01Screen *s = ui_edit_map_screen(ui);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old = s->attrs[cell];
            s->attrs[cell] =
                r01_attr_merge(old, r01_attr_bank(old), pal, r01_attr_flip_h(old), r01_attr_flip_v(old));
        }
    }
    screen_refresh_sel(ui);
}

void screen_remove_sel_tiles(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    R01Screen *s = ui_edit_map_screen(ui);
    int min_x, min_y, max_x, max_y, ty, tx;
    int touched = 0;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    (void)ui_undo_paint_begin(ui);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old_tile = s->tiles[cell];
            uint8_t old_attr = s->attrs[cell];
            if (old_tile == 0) {
                continue;
            }
            ui_undo_paint_record_cell(ui, tx, ty, old_tile, old_attr, 0, old_attr);
            r01_screen_paint_tile(p, s, tx, ty, 0, old_attr);
            touched = 1;
        }
    }
    ui_undo_paint_end(ui);
    if (touched) {
        ui_toast(ui, "tile removed", 0);
    }
}

void screen_toggle_sel_flag(UiState *ui, uint8_t flag) {
    R01Screen *s = ui_edit_map_screen(ui);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            s->attrs[cell] ^= flag;
        }
    }
}

void screen_set_solid_by_hw(UiState *ui, int ref_tx, int ref_ty) {
    R01Project *p = ui->project;
    R01Screen *s = ui_edit_map_screen(ui);
    int cell;
    int bank;
    int tile;
    int now_on;
    char msg[64];

    if (!p || !s || ref_tx < 0 || ref_tx >= R01_SCREEN_TILES_X || ref_ty < 0 || ref_ty >= R01_SCREEN_TILES_Y) {
        return;
    }
    cell = ref_ty * R01_SCREEN_TILES_X + ref_tx;
    bank = r01_attr_solid_bank(s->attrs[cell]);
    tile = (int)s->tiles[cell];
    now_on = r01_project_toggle_pattern_solid(p, bank, tile);
    screen_refresh_sel(ui);
    snprintf(msg, sizeof(msg), now_on ? "solid pattern bank %d tile %d" : "cleared solid bank %d tile %d", bank,
             tile);
    ui_toast(ui, msg, 0);
}
