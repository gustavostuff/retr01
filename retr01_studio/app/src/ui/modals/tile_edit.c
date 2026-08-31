#include "ui/ui.h"
#include "ui/internal.h"
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

void tile_edit_open(UiState *ui, int tx, int ty) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = ui_edit_map_screen(ui);
    int cell;
    uint8_t attr;
    memset(&ui->tile_edit, 0, sizeof(ui->tile_edit));
    ui->tile_edit.open = 1;
    ui->tile_edit.paint_tx = tx;
    ui->tile_edit.paint_ty = ty;
    ui->tile_edit.bank = 0;
    ui->tile_edit.pal = 0;
    ui->tile_edit.color = 1;
    ui->tile_edit.edit_all = 0;
    if (s && w && tx >= 0 && ty >= 0) {
        cell = ty * R01_SCREEN_TILES_X + tx;
        attr = s->attrs[cell];
        ui->tile_edit.tile_id = s->tiles[cell];
        ui->tile_edit.match_tile_id = s->tiles[cell];
        ui->tile_edit.match_attr_hw = r01_attr_hw(attr);
        ui->tile_edit.pal = r01_attr_pal(attr);
        ui->tile_edit.bank = r01_attr_bank(attr);
        ui->tile_edit.flip_h = r01_attr_flip_h(attr);
        ui->tile_edit.flip_v = r01_attr_flip_v(attr);
        if (ui->tile_edit.tile_id < w->bg_banks[ui->tile_edit.bank].tile_count) {
            const uint8_t *raw =
                w->bg_banks[ui->tile_edit.bank].chr + (size_t)ui->tile_edit.tile_id * R01_TILE_BYTES;
            r01_tile_orient(raw, ui->tile_edit.flip_h, ui->tile_edit.flip_v, ui->tile_edit.chr);
            ui->tile_edit.is_new = 0;
        } else {
            ui->tile_edit.is_new = 1;
            ui->tile_edit.tile_id = -1;
            memset(ui->tile_edit.chr, 0, sizeof(ui->tile_edit.chr));
        }
    } else {
        ui->tile_edit.is_new = 1;
        ui->tile_edit.tile_id = -1;
    }
}

void tile_edit_open_all(UiState *ui, int tx, int ty) {
    tile_edit_open(ui, tx, ty);
    ui->tile_edit.edit_all = 1;
}

void tile_edit_open_new(UiState *ui, int tx, int ty) {
    tile_edit_open(ui, tx, ty);
    ui->tile_edit.is_new = 1;
    ui->tile_edit.tile_id = -1;
    ui->tile_edit.bank = 0;
    ui->tile_edit.pal = 0;
    ui->tile_edit.flip_h = 0;
    ui->tile_edit.flip_v = 0;
    ui->tile_edit.edit_all = 0;
    memset(ui->tile_edit.chr, 0, sizeof(ui->tile_edit.chr));
}

static int tile_edit_apply_matching(R01World *w, R01Screen *s, int id, int bank, int pal, int flip_h,
                                    int flip_v, uint8_t match_tile_id, uint8_t match_attr_hw) {
    int cell;
    int touched = 0;
    if (!w || !s || !s->present) {
        return 0;
    }
    for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
        if (s->tiles[cell] != match_tile_id) {
            continue;
        }
        if (r01_attr_hw(s->attrs[cell]) != match_attr_hw) {
            continue;
        }
        s->tiles[cell] = (uint8_t)id;
        s->attrs[cell] = r01_attr_merge(s->attrs[cell], bank, pal, flip_h, flip_v);
        touched++;
    }
    return touched;
}

static void tile_edit_refresh_screens(R01World *w) {
    int si;
    if (!w) {
        return;
    }
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            r01_screen_fill_pixels_from_bank(w, &w->screens[si]);
        }
    }
    for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
        if (w->bg0_screens[si].present) {
            r01_screen_fill_pixels_from_bank(w, &w->bg0_screens[si]);
        }
    }
}

static void tile_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s;
    int id;
    int si;
    int touched = 0;
    int edit_all;
    uint8_t canonical[R01_TILE_BYTES];
    if (!w) {
        return;
    }
    edit_all = ui->tile_edit.edit_all;
    if (ui->tile_edit.is_new || ui->tile_edit.tile_id < 0) {
        id = r01_chr_alloc_tile(w, ui->tile_edit.bank);
        if (id < 0) {
            ui_toast(ui, "CHR bank full", 1);
            return;
        }
        ui->tile_edit.tile_id = id;
        ui->tile_edit.is_new = 0;
    } else {
        id = ui->tile_edit.tile_id;
    }
    r01_tile_orient(ui->tile_edit.chr, ui->tile_edit.flip_h, ui->tile_edit.flip_v, canonical);
    r01_chr_write_tile(w, ui->tile_edit.bank, id, canonical);

    if (edit_all) {
        for (si = 0; si < w->screen_count; si++) {
            touched += tile_edit_apply_matching(w, &w->screens[si], id, ui->tile_edit.bank, ui->tile_edit.pal,
                                                ui->tile_edit.flip_h, ui->tile_edit.flip_v,
                                                ui->tile_edit.match_tile_id, ui->tile_edit.match_attr_hw);
        }
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            touched += tile_edit_apply_matching(w, &w->bg0_screens[si], id, ui->tile_edit.bank, ui->tile_edit.pal,
                                                ui->tile_edit.flip_h, ui->tile_edit.flip_v,
                                                ui->tile_edit.match_tile_id, ui->tile_edit.match_attr_hw);
        }
        tile_edit_refresh_screens(w);
    } else {
        tile_edit_refresh_screens(w);
        s = ui_edit_map_screen(ui);
        if (s && ui->tile_edit.paint_tx >= 0 && ui->tile_edit.paint_ty >= 0) {
            r01_screen_paint_tile(w, s, ui->tile_edit.paint_tx, ui->tile_edit.paint_ty, (uint8_t)id,
                                  r01_attr_pack(ui->tile_edit.bank, ui->tile_edit.pal, ui->tile_edit.flip_h,
                                                ui->tile_edit.flip_v));
            touched = 1;
        }
    }

    ui->brush.armed = 1;
    ui->brush.bank = ui->tile_edit.bank;
    ui->brush.tile_id = id;
    ui->brush.pal = ui->tile_edit.pal;
    ui->brush.flip_h = ui->tile_edit.flip_h;
    ui->brush.flip_v = ui->tile_edit.flip_v;
    memcpy(ui->brush.chr, ui->tile_edit.chr, R01_TILE_BYTES);
    ui_paint_stamp_set(ui, (uint8_t)id,
                       r01_attr_pack(ui->tile_edit.bank, ui->tile_edit.pal, ui->tile_edit.flip_h,
                                     ui->tile_edit.flip_v));
    ui->tile_edit.open = 0;
    if (edit_all) {
        char msg[64];
        snprintf(msg, sizeof(msg), "tile saved (%d cells)", touched);
        ui_toast(ui, msg, 0);
    } else {
        ui_toast(ui, "tile saved", 0);
    }
}

void draw_tile_modal(UiState *ui, SDL_Renderer *r) {
    TileModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = w ? w->default_pal_row : 0;
    int sy, sx;

    tile_modal_layout(ui, &lo);
    ui_modal_scrim(r, ui);
    ui_modal_panel(r, lo.mx, lo.my, UI_MODAL_W, UI_MODAL_H,
                   ui->tile_edit.edit_all ? "Edit tile (all)" : "Edit tile");

    draw_label(r, lo.pal_x, lo.pal_label_y, "Palette/color");
    ui_palette_grid_draw(r, ui->project, row, lo.pal_x, lo.pal_y, ui->tile_edit.pal, ui->tile_edit.color,
                         UI_PAL_PLANE_BG);

    fill_rect(r, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS, UI_COL_WELL_R, UI_COL_WELL_G,
              UI_COL_WELL_B);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(ui->tile_edit.chr, sx, sy);
            uint8_t cr, cg, cb;
            int cell = 16;
            r01_kit_rgb(ui->project->global_pal_bg[row][ui->tile_edit.pal].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, lo.canvas_x + sx * cell, lo.canvas_y + sy * cell, cell - 1, cell - 1, cr, cg, cb);
        }
    }

    ui_modal_save_cancel(r, lo.pal_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
}

int tile_modal_handle(UiState *ui, int lx, int ly, int down) {
    TileModalLayout lo;
    int pal, col;
    tile_modal_layout(ui, &lo);

    if (!down) {
        return 1;
    }
    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, UI_MODAL_W, UI_MODAL_H)) {
        ui->tile_edit.open = 0;
        return 1;
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui->tile_edit.color = col;
        ui->tile_edit.pal = pal;
        return 1;
    }
    if (lx >= lo.canvas_x && lx < lo.canvas_x + UI_TILE_CANVAS && ly >= lo.canvas_y &&
        ly < lo.canvas_y + UI_TILE_CANVAS) {
        int sx = (lx - lo.canvas_x) / 16;
        int sy = (ly - lo.canvas_y) / 16;
        r01_tile_set_pixel(ui->tile_edit.chr, sx, sy, (uint8_t)ui->tile_edit.color);
        return 1;
    }
    if (ui_modal_save_hit(lx, ly, lo.pal_x, lo.btn_y, lo.save_w)) {
        tile_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.pal_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui->tile_edit.open = 0;
        return 1;
    }
    return 1;
}
