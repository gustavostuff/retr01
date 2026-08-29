#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdio.h>
#include <string.h>

static void draw_bank_tile(UiState *ui, SDL_Renderer *r, int bank, int tile_id, int dx, int dy, int scale) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const uint8_t *tile;
    int row = w ? w->default_pal_row : 0;
    int sy, sx;
    if (!w) {
        return;
    }
    tile = r01_chr_spr_tile(w, bank, tile_id);
    if (!tile) {
        return;
    }
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(tile, sx, sy);
            uint8_t cr, cg, cb;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][0].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, dx + sx * scale, dy + sy * scale, scale, scale, cr, cg, cb);
        }
    }
}

static void draw_part(UiState *ui, SDL_Renderer *r, const R01EntityPart *pt, int ox, int oy, int scale,
                      int selected) {
    const R01World *w = r01_project_active_world_const(ui->project);
    uint8_t oriented[R01_TILE_BYTES];
    const uint8_t *raw;
    int row = w ? w->default_pal_row : 0;
    int sy, sx;
    if (!w || !pt) {
        return;
    }
    raw = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!raw) {
        return;
    }
    r01_tile_orient(raw, pt->flip_h, pt->flip_v, oriented);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(oriented, sx, sy);
            uint8_t cr, cg, cb;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, ox + (pt->dx + sx) * scale, oy + (pt->dy + sy) * scale, scale, scale, cr, cg, cb);
        }
    }
    if (selected) {
        draw_rect(r, ox + pt->dx * scale, oy + pt->dy * scale, 8 * scale, 8 * scale, 240, 240, 240);
    }
}

static int clamp_compose(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > R01_ENTITY_COMPOSE_PX - 8) {
        return R01_ENTITY_COMPOSE_PX - 8;
    }
    return v;
}

static int part_at(const R01EntityFrame *fr, int px, int py, int prefer_sel) {
    int i;
    if (!fr) {
        return -1;
    }
    if (prefer_sel >= 0 && prefer_sel < fr->part_count) {
        const R01EntityPart *pt = &fr->parts[prefer_sel];
        if (px >= pt->dx && px < pt->dx + 8 && py >= pt->dy && py < pt->dy + 8) {
            return prefer_sel;
        }
    }
    for (i = fr->part_count - 1; i >= 0; i--) {
        const R01EntityPart *pt = &fr->parts[i];
        if (px >= pt->dx && px < pt->dx + 8 && py >= pt->dy && py < pt->dy + 8) {
            return i;
        }
    }
    return -1;
}

static void paint_selected_part(UiState *ui, int cx, int cy) {
    R01World *w;
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    R01EntityPart *pt;
    const uint8_t *src;
    uint8_t tile[R01_TILE_BYTES];
    int lx, ly;
    int sel = ui->metasprite_edit.sel_part;
    if (!fr || sel < 0 || sel >= fr->part_count) {
        return;
    }
    pt = &fr->parts[sel];
    if (cx < pt->dx || cx >= pt->dx + 8 || cy < pt->dy || cy >= pt->dy + 8) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return;
    }
    src = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!src) {
        return;
    }
    memcpy(tile, src, R01_TILE_BYTES);
    lx = cx - pt->dx;
    ly = cy - pt->dy;
    if (pt->flip_h) {
        lx = 7 - lx;
    }
    if (pt->flip_v) {
        ly = 7 - ly;
    }
    r01_tile_set_pixel(tile, lx, ly, (uint8_t)(ui->metasprite_edit.paint_color & 3));
    (void)r01_chr_write_spr_tile(w, pt->bank, pt->tile_id, tile);
}

void metasprite_edit_open_new(UiState *ui) {
    if (!ui) {
        return;
    }
    memset(&ui->metasprite_edit, 0, sizeof(ui->metasprite_edit));
    ui->metasprite_edit.open = 1;
    ui->metasprite_edit.is_new = 1;
    ui->metasprite_edit.meta_idx = -1;
    r01_metasprite_init(&ui->metasprite_edit.draft, "Meta");
    ui->metasprite_edit.bank = 0;
    ui->metasprite_edit.sel_part = -1;
    ui->metasprite_edit.paint_color = 1;
    ui->metasprite_edit.paint_pal = 0;
}

void metasprite_edit_open(UiState *ui, int meta_idx) {
    R01World *w;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || meta_idx < 0 || meta_idx >= w->metasprite_count) {
        return;
    }
    memset(&ui->metasprite_edit, 0, sizeof(ui->metasprite_edit));
    ui->metasprite_edit.open = 1;
    ui->metasprite_edit.is_new = 0;
    ui->metasprite_edit.meta_idx = meta_idx;
    ui->metasprite_edit.draft = w->metasprites[meta_idx];
    ui->metasprite_edit.bank = 0;
    ui->metasprite_edit.sel_part = -1;
    ui->metasprite_edit.paint_color = 1;
    ui->metasprite_edit.paint_pal = 0;
}

static void metasprite_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    if (ui->metasprite_edit.is_new || ui->metasprite_edit.meta_idx < 0) {
        idx = r01_world_metasprite_add(w);
        if (idx < 0) {
            ui_toast(ui, "metasprite catalog full", 1);
            return;
        }
        w->metasprites[idx] = ui->metasprite_edit.draft;
        ui->metasprite_edit.meta_idx = idx;
        ui->metasprite_edit.is_new = 0;
        ui_toast(ui, "metasprite created", 0);
    } else {
        if (ui->metasprite_edit.meta_idx >= w->metasprite_count) {
            ui_toast(ui, "bad metasprite index", 1);
            return;
        }
        w->metasprites[ui->metasprite_edit.meta_idx] = ui->metasprite_edit.draft;
        ui_toast(ui, "metasprite saved", 0);
    }
    ui->metasprite_edit.open = 0;
}

void draw_metasprite_modal(UiState *ui, SDL_Renderer *r) {
    MetaspriteModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    int tx, ty, i;
    int row = w ? w->default_pal_row : 0;
    char spr_label[16];
    const char *title = ui->metasprite_edit.is_new ? "Add metasprite" : "Edit metasprite";

    metasprite_modal_layout(&lo);
    fill_rect(r, 0, 0, UI_LOGIC_W, UI_LOGIC_H, 0, 0, 0);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    {
        SDL_Rect full = {0, 0, UI_LOGIC_W, UI_LOGIC_H};
        SDL_RenderFillRect(r, &full);
    }
    fill_rect(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B);
    draw_rect(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    font_draw_centered(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_BTN_H, title, 240, 240, 240);

    font_draw(r, lo.left_grid_x, lo.left_label_y + 4, "Sprite bank", 230, 230, 230);
    draw_dot_strip(r, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, ui->metasprite_edit.bank, R01_SPR_BANKS);
    fill_rect(r, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    for (ty = 0; ty < 16; ty++) {
        for (tx = 0; tx < 16; tx++) {
            draw_bank_tile(ui, r, ui->metasprite_edit.bank, ty * 16 + tx, lo.left_grid_x + tx * 8,
                           lo.left_grid_y + ty * 8, 1);
        }
    }

    fill_rect(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    for (i = 1; i < 16; i++) {
        int g = i * 8;
        SDL_SetRenderDrawColor(r, 50, 50, 58, 255);
        {
            SDL_Rect hr = {lo.right_grid_x, lo.right_grid_y + g, UI_ENTITY_COMPOSE, 1};
            SDL_Rect vr = {lo.right_grid_x + g, lo.right_grid_y, 1, UI_ENTITY_COMPOSE};
            SDL_RenderFillRect(r, &hr);
            SDL_RenderFillRect(r, &vr);
        }
    }
    {
        int sel = ui->metasprite_edit.sel_part;
        for (i = 0; i < fr->part_count; i++) {
            if (i == sel) {
                continue;
            }
            draw_part(ui, r, &fr->parts[i], lo.right_grid_x, lo.right_grid_y, 8, 0);
        }
        if (sel >= 0 && sel < fr->part_count) {
            draw_part(ui, r, &fr->parts[sel], lo.right_grid_x, lo.right_grid_y, 8, 1);
        }
    }
    if (ui->metasprite_edit.dragging == 4) {
        draw_bank_tile(ui, r, ui->metasprite_edit.bank, ui->metasprite_edit.drag_tile,
                       ui->mouse_x - ui->metasprite_edit.drag_off_x, ui->mouse_y - ui->metasprite_edit.drag_off_y,
                       1);
    }

    snprintf(spr_label, sizeof(spr_label), "SPR %d", row);
    font_draw(r, lo.pal_label_x, lo.pal_label_y + 4, spr_label, 230, 230, 230);
    draw_spr_palette_grid(r, ui->project, row, lo.pal_x, lo.pal_y, ui->metasprite_edit.paint_pal,
                          ui->metasprite_edit.paint_color);

    if (ui->metasprite_edit.dragging == 5) {
        draw_brush_preview(r, ui->project, row, ui->metasprite_edit.paint_pal, ui->metasprite_edit.paint_color,
                           ui->mouse_x, ui->mouse_y);
    }

    {
        int save_hover = point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_grid_x, lo.btn_y, lo.save_w, UI_BTN_H);
        int cancel_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w,
                          UI_BTN_H);
        draw_button(r, lo.left_grid_x, lo.btn_y, lo.save_w, "Save", 1, save_hover);
        draw_button(r, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, "Cancel", 0, cancel_hover);
    }
}

int metasprite_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    MetaspriteModalLayout lo;
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);

    metasprite_modal_layout(&lo);

    if (!down) {
        if (!right && ui->metasprite_edit.dragging == 4 &&
            point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
            R01EntityPart part;
            int cx = (lx - lo.right_grid_x) / 8;
            int cy = (ly - lo.right_grid_y) / 8;
            memset(&part, 0, sizeof(part));
            part.bank = ui->metasprite_edit.bank;
            part.tile_id = ui->metasprite_edit.drag_tile;
            part.pal = ui->metasprite_edit.paint_pal;
            {
                const R01World *w = r01_project_active_world_const(ui->project);
                int si;
                if (w) {
                    for (si = 0; si < w->sprite_count; si++) {
                        if (w->sprites[si].bank == part.bank && w->sprites[si].tile_id == part.tile_id) {
                            part.pal = w->sprites[si].pal;
                            break;
                        }
                    }
                }
            }
            part.dx = clamp_compose(cx - 4);
            part.dy = clamp_compose(cy - 4);
            idx = r01_metasprite_add_part(&ui->metasprite_edit.draft, &part);
            if (idx >= 0) {
                ui->metasprite_edit.sel_part = idx;
            } else {
                ui_toast(ui, "part limit", 1);
            }
        }
        ui->metasprite_edit.dragging = 0;
        return 1;
    }

    if (spr_palette_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui->metasprite_edit.paint_pal = pal;
        ui->metasprite_edit.paint_color = col;
        return 1;
    }
    if (dot_strip_hit(lx, ly, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, &idx)) {
        ui->metasprite_edit.bank = idx;
        return 1;
    }
    if (point_in_rect(lx, ly, lo.left_grid_x, lo.btn_y, lo.save_w, UI_BTN_H)) {
        metasprite_edit_save(ui);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H)) {
        ui->metasprite_edit.open = 0;
        return 1;
    }
    if (!right && point_in_rect(lx, ly, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID)) {
        int txx = (lx - lo.left_grid_x) / 8;
        int tyy = (ly - lo.left_grid_y) / 8;
        ui->metasprite_edit.dragging = 4;
        ui->metasprite_edit.drag_tile = tyy * 16 + txx;
        ui->metasprite_edit.drag_off_x = lx - (lo.left_grid_x + txx * 8);
        ui->metasprite_edit.drag_off_y = ly - (lo.left_grid_y + tyy * 8);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        if (right) {
            idx = part_at(fr, cx, cy, ui->metasprite_edit.sel_part);
            if (idx >= 0) {
                if (idx != ui->metasprite_edit.sel_part) {
                    ui->metasprite_edit.sel_part = idx;
                    return 1;
                }
                paint_selected_part(ui, cx, cy);
                ui->metasprite_edit.dragging = 5;
            } else {
                ui->metasprite_edit.sel_part = -1;
            }
        } else {
            idx = part_at(fr, cx, cy, ui->metasprite_edit.sel_part);
            if (idx >= 0) {
                ui->metasprite_edit.sel_part = idx;
                ui->metasprite_edit.dragging = 1;
                ui->metasprite_edit.drag_off_x = cx - fr->parts[idx].dx;
                ui->metasprite_edit.drag_off_y = cy - fr->parts[idx].dy;
            } else {
                ui->metasprite_edit.sel_part = -1;
            }
        }
        return 1;
    }
    return 1;
}

void metasprite_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons) {
    MetaspriteModalLayout lo;
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    if (!ui || !ui->metasprite_edit.open || !ui->metasprite_edit.dragging) {
        return;
    }
    metasprite_modal_layout(&lo);
    if (ui->metasprite_edit.dragging == 5 && (buttons & SDL_BUTTON_RMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        paint_selected_part(ui, cx, cy);
    } else if (ui->metasprite_edit.dragging == 1 && ui->metasprite_edit.sel_part >= 0 &&
               ui->metasprite_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        fr->parts[ui->metasprite_edit.sel_part].dx = clamp_compose(cx - ui->metasprite_edit.drag_off_x);
        fr->parts[ui->metasprite_edit.sel_part].dy = clamp_compose(cy - ui->metasprite_edit.drag_off_y);
    }
}

void metasprite_modal_key(UiState *ui, SDL_Keycode sym) {
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    R01EntityPart *pt;
    if (!ui || !ui->metasprite_edit.open) {
        return;
    }
    if (sym >= SDLK_1 && sym <= SDLK_4) {
        ui->metasprite_edit.paint_color = (int)(sym - SDLK_1);
        return;
    }
    if (ui->metasprite_edit.sel_part < 0 || ui->metasprite_edit.sel_part >= fr->part_count) {
        return;
    }
    pt = &fr->parts[ui->metasprite_edit.sel_part];
    if (sym == SDLK_h) {
        pt->flip_h = !pt->flip_h;
    } else if (sym == SDLK_v) {
        pt->flip_v = !pt->flip_v;
    } else if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
        r01_metasprite_remove_part(&ui->metasprite_edit.draft, ui->metasprite_edit.sel_part);
        ui->metasprite_edit.sel_part = -1;
    }
}
