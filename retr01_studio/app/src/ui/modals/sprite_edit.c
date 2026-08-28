#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <string.h>

void sprite_edit_open_new(UiState *ui) {
    R01World *w;
    int bank;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return;
    }
    bank = r01_chr_find_spr_bank_space(w);
    if (bank < 0) {
        ui_toast(ui, "all sprite banks full", 1);
        return;
    }
    memset(&ui->sprite_edit, 0, sizeof(ui->sprite_edit));
    ui->sprite_edit.open = 1;
    ui->sprite_edit.is_new = 1;
    ui->sprite_edit.catalog_idx = -1;
    ui->sprite_edit.bank = bank;
    ui->sprite_edit.tile_id = -1;
    ui->sprite_edit.pal = 0;
    ui->sprite_edit.color = 1;
    ui->sprite_edit.flip_h = 0;
    ui->sprite_edit.flip_v = 0;
    memset(ui->sprite_edit.chr, 0, sizeof(ui->sprite_edit.chr));
}

void sprite_edit_open(UiState *ui, int catalog_idx) {
    R01World *w;
    const R01SpriteDef *sp;
    const uint8_t *raw;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return;
    }
    sp = &w->sprites[catalog_idx];
    memset(&ui->sprite_edit, 0, sizeof(ui->sprite_edit));
    ui->sprite_edit.open = 1;
    ui->sprite_edit.is_new = 0;
    ui->sprite_edit.catalog_idx = catalog_idx;
    ui->sprite_edit.bank = sp->bank;
    ui->sprite_edit.tile_id = sp->tile_id;
    ui->sprite_edit.pal = sp->pal;
    ui->sprite_edit.color = 1;
    ui->sprite_edit.flip_h = 0;
    ui->sprite_edit.flip_v = 0;
    raw = r01_chr_spr_tile(w, sp->bank, sp->tile_id);
    if (raw) {
        memcpy(ui->sprite_edit.chr, raw, R01_TILE_BYTES);
    } else {
        memset(ui->sprite_edit.chr, 0, sizeof(ui->sprite_edit.chr));
    }
}

static void sprite_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    uint8_t canonical[R01_TILE_BYTES];
    int id;
    int cat;
    if (!w) {
        return;
    }
    r01_tile_orient(ui->sprite_edit.chr, ui->sprite_edit.flip_h, ui->sprite_edit.flip_v, canonical);
    if (ui->sprite_edit.is_new || ui->sprite_edit.tile_id < 0) {
        id = r01_chr_alloc_spr_tile(w, ui->sprite_edit.bank);
        if (id < 0) {
            /* Prefer next bank if current filled while modal was open. */
            int bank = r01_chr_find_spr_bank_space(w);
            if (bank < 0) {
                ui_toast(ui, "sprite banks full", 1);
                return;
            }
            ui->sprite_edit.bank = bank;
            id = r01_chr_alloc_spr_tile(w, bank);
            if (id < 0) {
                ui_toast(ui, "sprite banks full", 1);
                return;
            }
        }
        ui->sprite_edit.tile_id = id;
        if (r01_chr_write_spr_tile(w, ui->sprite_edit.bank, id, canonical) != 0) {
            ui_toast(ui, "sprite write failed", 1);
            return;
        }
        cat = r01_world_sprite_add(w, ui->sprite_edit.bank, id, ui->sprite_edit.pal);
        if (cat < 0) {
            ui_toast(ui, "sprite catalog full", 1);
            return;
        }
        ui->sprite_edit.catalog_idx = cat;
        ui->sprite_edit.is_new = 0;
        ui_toast(ui, "sprite created", 0);
    } else {
        id = ui->sprite_edit.tile_id;
        if (r01_chr_write_spr_tile(w, ui->sprite_edit.bank, id, canonical) != 0) {
            ui_toast(ui, "sprite write failed", 1);
            return;
        }
        if (ui->sprite_edit.catalog_idx >= 0 && ui->sprite_edit.catalog_idx < w->sprite_count) {
            w->sprites[ui->sprite_edit.catalog_idx].pal = ui->sprite_edit.pal;
            w->sprites[ui->sprite_edit.catalog_idx].bank = ui->sprite_edit.bank;
            w->sprites[ui->sprite_edit.catalog_idx].tile_id = id;
        }
        ui_toast(ui, "sprite saved", 0);
    }
    ui->sprite_edit.open = 0;
}

void draw_sprite_modal(UiState *ui, SDL_Renderer *r) {
    SpriteModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = w ? w->default_pal_row : 0;
    int pal, c, sy, sx;
    const char *title = ui->sprite_edit.is_new ? "Create sprite" : "Edit sprite";

    sprite_modal_layout(&lo);
    fill_rect(r, 0, 0, UI_LOGIC_W, UI_LOGIC_H, 0, 0, 0);
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        {
            SDL_Rect full = {0, 0, UI_LOGIC_W, UI_LOGIC_H};
            SDL_RenderFillRect(r, &full);
        }
    }
    fill_rect(r, lo.mx, lo.my, UI_MODAL_W, UI_MODAL_H, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B);
    draw_rect(r, lo.mx, lo.my, UI_MODAL_W, UI_MODAL_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);

    font_draw_centered(r, lo.mx, lo.my, UI_MODAL_W, UI_BTN_H, title, 240, 240, 240);

    draw_label(r, lo.pal_x, lo.pal_label_y, "Palette/color");
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            int x = lo.pal_x + c * UI_PAL_SWATCH;
            int y = lo.pal_y + pal * UI_PAL_SWATCH;
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, cr, cg, cb);
            if (pal == ui->sprite_edit.pal && c == ui->sprite_edit.color) {
                draw_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, 240, 240, 240);
            }
        }
    }

    fill_rect(r, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS, UI_COL_WELL_R, UI_COL_WELL_G,
              UI_COL_WELL_B);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(ui->sprite_edit.chr, sx, sy);
            uint8_t cr, cg, cb;
            int cell = 16;
            r01_kit_rgb(ui->project->global_pal_spr[row][ui->sprite_edit.pal].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, lo.canvas_x + sx * cell, lo.canvas_y + sy * cell, cell - 1, cell - 1, cr, cg, cb);
        }
    }

    {
        int save_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, lo.pal_x, lo.btn_y, lo.save_w, UI_BTN_H);
        int cancel_hover = point_in_rect(ui->mouse_x, ui->mouse_y, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y,
                                         lo.cancel_w, UI_BTN_H);
        draw_button(r, lo.pal_x, lo.btn_y, lo.save_w, "Save", 1, save_hover);
        draw_button(r, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, "Cancel", 0, cancel_hover);
    }
}

int sprite_modal_handle(UiState *ui, int lx, int ly, int down) {
    SpriteModalLayout lo;
    sprite_modal_layout(&lo);

    if (!down) {
        return 1;
    }
    if (lx >= lo.pal_x && lx < lo.pal_x + 4 * UI_PAL_SWATCH && ly >= lo.pal_y &&
        ly < lo.pal_y + 4 * UI_PAL_SWATCH) {
        ui->sprite_edit.color = (lx - lo.pal_x) / UI_PAL_SWATCH;
        ui->sprite_edit.pal = (ly - lo.pal_y) / UI_PAL_SWATCH;
        return 1;
    }
    if (lx >= lo.canvas_x && lx < lo.canvas_x + UI_TILE_CANVAS && ly >= lo.canvas_y &&
        ly < lo.canvas_y + UI_TILE_CANVAS) {
        int sx = (lx - lo.canvas_x) / 16;
        int sy = (ly - lo.canvas_y) / 16;
        r01_tile_set_pixel(ui->sprite_edit.chr, sx, sy, (uint8_t)ui->sprite_edit.color);
        return 1;
    }
    if (lx >= lo.pal_x && lx < lo.pal_x + lo.save_w && ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        sprite_edit_save(ui);
        return 1;
    }
    if (lx >= lo.pal_x + lo.save_w + UI_UNIT && lx < lo.pal_x + lo.save_w + UI_UNIT + lo.cancel_w &&
        ly >= lo.btn_y && ly < lo.btn_y + UI_BTN_H) {
        ui->sprite_edit.open = 0;
        return 1;
    }
    return 1;
}
