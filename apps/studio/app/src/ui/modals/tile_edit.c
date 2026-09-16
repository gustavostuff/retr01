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

#define UI_TILE_EDIT_UNDO_MAX 32

static void tile_edit_stroke_begin(UiState *ui) {
    if (!ui || !ui->tile_edit.open || ui->tile_edit.stroke_open) {
        return;
    }
    memcpy(ui->tile_edit.stroke_before, ui->tile_edit.chr, R01_TILE_BYTES);
    ui->tile_edit.stroke_open = 1;
    ui->tile_edit.stroke_dirty = 0;
}

void tile_modal_stroke_end(UiState *ui) {
    UiTileEdit *te;
    int i;
    if (!ui || !ui->tile_edit.open || !ui->tile_edit.stroke_open) {
        return;
    }
    te = &ui->tile_edit;
    te->stroke_open = 0;
    if (!te->stroke_dirty || memcmp(te->stroke_before, te->chr, R01_TILE_BYTES) == 0) {
        te->stroke_dirty = 0;
        return;
    }
    if (te->undo_cursor < te->undo_count) {
        te->undo_count = te->undo_cursor;
    }
    if (te->undo_count >= UI_TILE_EDIT_UNDO_MAX) {
        for (i = 1; i < UI_TILE_EDIT_UNDO_MAX; i++) {
            memcpy(te->undo_before[i - 1], te->undo_before[i], R01_TILE_BYTES);
            memcpy(te->undo_after[i - 1], te->undo_after[i], R01_TILE_BYTES);
        }
        te->undo_count = UI_TILE_EDIT_UNDO_MAX - 1;
        te->undo_cursor = te->undo_count;
    }
    memcpy(te->undo_before[te->undo_count], te->stroke_before, R01_TILE_BYTES);
    memcpy(te->undo_after[te->undo_count], te->chr, R01_TILE_BYTES);
    te->undo_count++;
    te->undo_cursor = te->undo_count;
    te->stroke_dirty = 0;
}

int tile_edit_undo(UiState *ui) {
    UiTileEdit *te;
    if (!ui || !ui->tile_edit.open) {
        return 0;
    }
    tile_modal_stroke_end(ui);
    te = &ui->tile_edit;
    if (te->undo_cursor < 1) {
        return 0;
    }
    te->undo_cursor--;
    memcpy(te->chr, te->undo_before[te->undo_cursor], R01_TILE_BYTES);
    ui_toast(ui, "undo paint tile", 0);
    return 1;
}

int tile_edit_redo(UiState *ui) {
    UiTileEdit *te;
    if (!ui || !ui->tile_edit.open) {
        return 0;
    }
    tile_modal_stroke_end(ui);
    te = &ui->tile_edit;
    if (te->undo_cursor >= te->undo_count) {
        return 0;
    }
    memcpy(te->chr, te->undo_after[te->undo_cursor], R01_TILE_BYTES);
    te->undo_cursor++;
    ui_toast(ui, "redo paint tile", 0);
    return 1;
}

static void tile_edit_paint_pixel(UiState *ui, int sx, int sy) {
    uint8_t old;
    if (!ui || sx < 0 || sy < 0 || sx >= 8 || sy >= 8) {
        return;
    }
    tile_edit_stroke_begin(ui);
    old = r01_tile_pixel_color(ui->tile_edit.chr, sx, sy) & 3u;
    if (old == ((uint8_t)ui->tile_edit.color & 3u)) {
        return;
    }
    r01_tile_set_pixel(ui->tile_edit.chr, sx, sy, (uint8_t)ui->tile_edit.color);
    ui->tile_edit.stroke_dirty = 1;
}

static void tile_edit_flood(UiState *ui, int sx, int sy) {
    uint8_t before[R01_TILE_BYTES];
    if (!ui || sx < 0 || sy < 0 || sx >= 8 || sy >= 8) {
        return;
    }
    tile_modal_stroke_end(ui);
    memcpy(before, ui->tile_edit.chr, R01_TILE_BYTES);
    r01_tile_flood_fill(ui->tile_edit.chr, sx, sy, (uint8_t)ui->tile_edit.color);
    if (memcmp(before, ui->tile_edit.chr, R01_TILE_BYTES) == 0) {
        return;
    }
    memcpy(ui->tile_edit.stroke_before, before, R01_TILE_BYTES);
    ui->tile_edit.stroke_open = 1;
    ui->tile_edit.stroke_dirty = 1;
    tile_modal_stroke_end(ui);
}

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

void tile_edit_open_bank(UiState *ui, int bank, int tile_id, int is_new) {
    R01World *w;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    memset(&ui->tile_edit, 0, sizeof(ui->tile_edit));
    ui->tile_edit.open = 1;
    ui->tile_edit.paint_tx = -1;
    ui->tile_edit.paint_ty = -1;
    ui->tile_edit.bank = bank;
    ui->tile_edit.tile_id = tile_id;
    ui->tile_edit.pal = 0;
    ui->tile_edit.color = 1;
    ui->tile_edit.edit_all = 0;
    ui->tile_edit.is_new = is_new ? 1 : 0;
    if (w && bank >= 0 && bank < R01_BG_BANKS && tile_id >= 0 && tile_id < w->bg_banks[bank].tile_count) {
        const uint8_t *raw = w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
        memcpy(ui->tile_edit.chr, raw, R01_TILE_BYTES);
        ui->tile_edit.is_new = 0;
    } else {
        memset(ui->tile_edit.chr, 0, sizeof(ui->tile_edit.chr));
        ui->tile_edit.is_new = 1;
    }
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
    int was_new;
    int old_tile_count = 0;
    int painted = 0;
    uint8_t old_tile = 0, old_attr = 0, new_attr = 0;
    uint8_t old_chr[R01_TILE_BYTES];
    uint8_t canonical[R01_TILE_BYTES];
    if (!w) {
        return;
    }
    edit_all = ui->tile_edit.edit_all;
    was_new = ui->tile_edit.is_new || ui->tile_edit.tile_id < 0;
    memset(old_chr, 0, sizeof(old_chr));
    if (was_new) {
        old_tile_count = w->bg_banks[ui->tile_edit.bank].tile_count;
        id = r01_chr_alloc_tile(w, ui->tile_edit.bank);
        if (id < 0) {
            ui_toast(ui, "CHR bank full", 1);
            return;
        }
        ui->tile_edit.tile_id = id;
        ui->tile_edit.is_new = 0;
    } else {
        id = ui->tile_edit.tile_id;
        if (id >= 0 && id < w->bg_banks[ui->tile_edit.bank].tile_count) {
            memcpy(old_chr, w->bg_banks[ui->tile_edit.bank].chr + (size_t)id * R01_TILE_BYTES, R01_TILE_BYTES);
        }
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
            int cell = ui->tile_edit.paint_ty * R01_SCREEN_TILES_X + ui->tile_edit.paint_tx;
            old_tile = s->tiles[cell];
            old_attr = s->attrs[cell];
            new_attr = r01_attr_pack(ui->tile_edit.bank, ui->tile_edit.pal, ui->tile_edit.flip_h,
                                     ui->tile_edit.flip_v);
            r01_screen_paint_tile(w, s, ui->tile_edit.paint_tx, ui->tile_edit.paint_ty, (uint8_t)id, new_attr);
            painted = 1;
            touched = 1;
        }
    }

    if (was_new) {
        ui_undo_push_tile_create(ui, ui->tile_edit.bank, id, old_tile_count, painted, ui->tile_edit.paint_tx,
                                 ui->tile_edit.paint_ty, old_tile, old_attr, (uint8_t)id, new_attr);
    } else {
        ui_undo_push_bg_chr_edit(ui, ui->tile_edit.bank, id, old_chr, canonical);
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
    ui_modal_scrim(r, ui_logic_w(ui), ui_logic_h(ui));
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh,
                   ui->tile_edit.edit_all ? "Edit tile (all)" : "Edit tile");
#if UI_PANEL_DEBUG_GRID
    ui_panel_debug_draw(r, &lo.dbg_panel);
#endif

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
    if (!ui->menu.open && point_in_rect(ui->mouse_x, ui->mouse_y, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS,
                                       UI_TILE_CANVAS)) {
        int cell = 16;
        int hx = (ui->mouse_x - lo.canvas_x) / cell;
        int hy = (ui->mouse_y - lo.canvas_y) / cell;
        draw_paint_pixel_preview(r, ui->project, row, UI_PAL_PLANE_BG, ui->tile_edit.pal, ui->tile_edit.color,
                                 lo.canvas_x + hx * cell, lo.canvas_y + hy * cell, cell - 1);
    }

    ui_modal_save_cancel(r, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
}

int tile_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    TileModalLayout lo;
    int pal, col;
    tile_modal_layout(ui, &lo);

    if (!down) {
        tile_modal_stroke_end(ui);
        return 1;
    }
    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        tile_modal_stroke_end(ui);
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
        if (button == SDL_BUTTON_RIGHT) {
            ui->tile_edit.color = (int)(r01_tile_pixel_color(ui->tile_edit.chr, sx, sy) & 3u);
            return 1;
        }
        if (button != SDL_BUTTON_LEFT) {
            return 1;
        }
        if (ui->keys[SDL_SCANCODE_F]) {
            tile_edit_flood(ui, sx, sy);
        } else {
            tile_edit_paint_pixel(ui, sx, sy);
        }
        return 1;
    }
    if (ui_modal_save_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w)) {
        tile_modal_stroke_end(ui);
        tile_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        tile_modal_stroke_end(ui);
        ui->tile_edit.open = 0;
        return 1;
    }
    return 1;
}

static void tile_or_sprite_modal_layout(const UiState *ui, int *mx, int *my, int *mw, int *mh, int *pal_x,
                                        int *pal_label_y, int *pal_y, int *canvas_x, int *canvas_y, int *btn_y,
                                        int *save_w, int *cancel_w, int *left_btn_x, UiPanel *dbg) {
    enum { C_PAL_LAB = 1, C_PAL, C_CANVAS, C_FOOTER };
    static const UiPanelCell cells[] = {
        {C_PAL_LAB, 0, 0, 1, 1},
        {C_PAL, 0, 1, 1, 1},
        {C_CANVAS, 2, 0, 1, 3},
        {C_FOOTER, 0, 4, 3, 1},
    };
    /* Match legacy UI_MODAL_W/H: 8+64+80+128+8 by 16+8+16+64+48+8+16+8. */
    static const int row_hs[] = {UI_BTN_H, UI_PAL_GRID_SIZE, 48, UI_UNIT, UI_BTN_H};
    UiPanel panel;
    int pad = UI_UNIT;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int gap = UI_MODAL_W - pad * 2 - UI_PAL_GRID_SIZE - UI_TILE_CANVAS;
    int i;

    if (gap < pad) {
        gap = pad;
    }
    ui_panel_init(&panel, 3, 5, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, (int)(sizeof(cells) / sizeof(cells[0])));
    ui_panel_set_col_w(&panel, 0, UI_PAL_GRID_SIZE);
    ui_panel_set_col_w(&panel, 1, gap);
    ui_panel_set_col_w(&panel, 2, UI_TILE_CANVAS);
    for (i = 0; i < (int)(sizeof(row_hs) / sizeof(row_hs[0])); i++) {
        ui_panel_set_row_h(&panel, i, row_hs[i]);
    }
    ui_panel_layout(&panel, 0, 0);

    *mw = pad + panel.total_w + pad;
    *mh = UI_BTN_H + pad + panel.total_h + pad;
    *mx = (ui_logic_w(ui) - *mw) / 2;
    *my = (ui_logic_h(ui) - *mh) / 2;
    content_x = *mx + pad;
    content_y = *my + UI_BTN_H + pad;
    ui_panel_layout(&panel, content_x, content_y);

    ui_panel_cell(&panel, C_PAL_LAB, &cx, &cy, &cw, &ch);
    *pal_x = cx;
    *pal_label_y = cy;
    ui_panel_cell(&panel, C_PAL, &cx, &cy, &cw, &ch);
    *pal_y = cy;
    ui_panel_cell(&panel, C_CANVAS, &cx, &cy, &cw, &ch);
    *canvas_x = cx;
    *canvas_y = cy;
    ui_panel_cell(&panel, C_FOOTER, &cx, &cy, &cw, &ch);
    *btn_y = cy;
    *left_btn_x = content_x;
    *save_w = label_width("Save");
    *cancel_w = label_width("Cancel");
#if UI_PANEL_DEBUG_GRID
    if (dbg) {
        *dbg = panel;
    }
#else
    (void)dbg;
#endif
}

void tile_modal_layout(const UiState *ui, TileModalLayout *lo) {
    tile_or_sprite_modal_layout(ui, &lo->mx, &lo->my, &lo->mw, &lo->mh, &lo->pal_x, &lo->pal_label_y, &lo->pal_y,
                                &lo->canvas_x, &lo->canvas_y, &lo->btn_y, &lo->save_w, &lo->cancel_w, &lo->left_btn_x,
#if UI_PANEL_DEBUG_GRID
                                &lo->dbg_panel
#else
                                NULL
#endif
    );
}

