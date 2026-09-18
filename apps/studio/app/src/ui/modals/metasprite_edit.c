#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/undo/undo_cmds.h"
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
    ui_text_blur(&ui->text);
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
    ui_text_blur(&ui->text);
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
        ui_undo_push_metasprite_add(ui, idx);
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
    ui_text_blur(&ui->text);
}

void draw_metasprite_modal(UiState *ui, SDL_Renderer *r) {
    MetaspriteModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    int tx, ty;
    int row = w ? w->default_pal_row : 0;
    char spr_label[16];
    const char *title = ui->metasprite_edit.is_new ? "Add metasprite" : "Edit metasprite";

    metasprite_modal_layout(ui, &lo);
    ui_modal_scrim(r, ui_logic_w(ui), ui_logic_h(ui));
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, title);
#if UI_PANEL_DEBUG_GRID
    ui_panel_debug_draw(r, &lo.dbg_panel);
#endif

    font_draw(r, lo.left_grid_x, lo.left_label_y + 4, "Sprite bank", 230, 230, 230);
    ui_dot_strip_draw(r, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, ui->metasprite_edit.bank, R01_SPR_BANKS);
    fill_rect(r, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    for (ty = 0; ty < 16; ty++) {
        for (tx = 0; tx < 16; tx++) {
            draw_bank_tile(ui, r, ui->metasprite_edit.bank, ty * 16 + tx, lo.left_grid_x + tx * 8,
                           lo.left_grid_y + ty * 8, 1);
        }
    }

    {
        const char *mname = r01_metasprite_display_name(&ui->metasprite_edit.draft);
        char mid[R01_ID_MAX];
        int wi = ui->project ? ui->project->active_world : 0;
        font_draw(r, lo.right_grid_x, lo.right_name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(&ui->text, r, lo.right_name_x, lo.right_name_y, lo.right_name_w, mname, 1);
        r01_metasprite_id(mid, sizeof(mid), wi, &ui->metasprite_edit.draft);
        font_draw_clipped(r, lo.right_grid_x, lo.right_id_y + 4, lo.right_grid_x, lo.right_id_y, lo.right_id_w,
                          UI_BTN_H, mid, 160, 160, 170);
    }

    ui_compose_draw_grid(r, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, 8);
    {
        unsigned mask = 0;
        if (ui->metasprite_edit.sel_part >= 0 && ui->metasprite_edit.sel_part < R01_ENTITY_PARTS_MAX) {
            mask = 1u << ui->metasprite_edit.sel_part;
        }
        ui_compose_draw_frame(r, ui->project, w, fr, lo.right_grid_x, lo.right_grid_y, 8, mask, 0, 255);
    }
    if (ui->metasprite_edit.dragging != 4 && !ui->menu.open &&
        point_in_rect(ui->mouse_x, ui->mouse_y, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE,
                      UI_METASPRITE_COMPOSE)) {
        int cx = (ui->mouse_x - lo.right_grid_x) / 8;
        int cy = (ui->mouse_y - lo.right_grid_y) / 8;
        int idx = ui_compose_part_at(fr, cx, cy, ui->metasprite_edit.sel_part);
        if (idx >= 0) {
            draw_paint_pixel_preview(r, ui->project, row, UI_PAL_PLANE_SPR, ui->metasprite_edit.paint_pal,
                                     ui->metasprite_edit.paint_color, lo.right_grid_x + cx * 8,
                                     lo.right_grid_y + cy * 8, 8);
        }
    }

    if (ui->metasprite_edit.dragging == 4) {
        draw_bank_tile(ui, r, ui->metasprite_edit.bank, ui->metasprite_edit.drag_tile,
                       ui->mouse_x - ui->metasprite_edit.drag_off_x, ui->mouse_y - ui->metasprite_edit.drag_off_y,
                       1);
    }

    snprintf(spr_label, sizeof(spr_label), "SPR %d", row);
    font_draw(r, lo.pal_label_x, lo.pal_label_y + 4, spr_label, 230, 230, 230);
    ui_palette_grid_draw(r, ui->project, row, lo.pal_x, lo.pal_y, ui->metasprite_edit.paint_pal,
                         ui->metasprite_edit.paint_color, UI_PAL_PLANE_SPR);

    ui_modal_save_cancel(r, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
}

int metasprite_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    MetaspriteModalLayout lo;
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);

    metasprite_modal_layout(ui, &lo);

    if (!down) {
        if (!right && ui->metasprite_edit.dragging == 4 &&
            point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, UI_METASPRITE_COMPOSE)) {
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
            part.dx = ui_compose_clamp_part(cx - 4);
            part.dy = ui_compose_clamp_part(cy - 4);
            idx = r01_metasprite_add_part(&ui->metasprite_edit.draft, &part);
            if (idx >= 0) {
                ui->metasprite_edit.sel_part = idx;
            } else {
                ui_toast(ui, "part limit", 1);
            }
        }
        ui->metasprite_edit.dragging = 0;
        ui_text_mouse_up(&ui->text);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        ui->metasprite_edit.open = 0;
        ui_text_blur(&ui->text);
        return 1;
    }

    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(&ui->text);
        ui->metasprite_edit.paint_pal = pal;
        ui->metasprite_edit.paint_color = col;
        return 1;
    }
    if (ui_text_mouse_down(&ui->text, lx, ly, lo.right_name_x, lo.right_name_y, lo.right_name_w,
                           ui->metasprite_edit.draft.name, R01_ENTITY_NAME_MAX, 1)) {
        return 1;
    }
    ui_text_blur(&ui->text);
    if (ui_dot_strip_hit(lx, ly, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, &idx)) {
        if (idx < R01_SPR_BANKS) {
            ui->metasprite_edit.bank = idx;
        }
        return 1;
    }
    if (ui_modal_save_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w)) {
        metasprite_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui->metasprite_edit.open = 0;
        ui_text_blur(&ui->text);
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
    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, UI_METASPRITE_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        if (right) {
            idx = ui_compose_part_at(fr, cx, cy, ui->metasprite_edit.sel_part);
            if (idx >= 0) {
                if (idx != ui->metasprite_edit.sel_part) {
                    ui->metasprite_edit.sel_part = idx;
                    return 1;
                }
                {
                    R01World *ww = r01_project_active_world(ui->project);
                    if (ww) {
                        ui_compose_paint_part(ui->project, ww, &fr->parts[idx], cx, cy,
                                              ui->metasprite_edit.paint_color);
                    }
                }
                ui->metasprite_edit.dragging = 5;
            } else {
                ui->metasprite_edit.sel_part = -1;
            }
        } else {
            idx = ui_compose_part_at(fr, cx, cy, ui->metasprite_edit.sel_part);
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
    if (!ui || !ui->metasprite_edit.open) {
        return;
    }
    metasprite_modal_layout(ui, &lo);
    if (ui->text.drag && ui->text.field_id == 1) {
        ui_text_mouse_drag(&ui->text, lx, lo.right_name_x, lo.right_name_w);
        return;
    }
    if (!ui->metasprite_edit.dragging) {
        return;
    }
    if (ui->metasprite_edit.dragging == 5 && (buttons & SDL_BUTTON_RMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, UI_METASPRITE_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        R01World *ww = r01_project_active_world(ui->project);
        if (ww && ui->metasprite_edit.sel_part >= 0 && ui->metasprite_edit.sel_part < fr->part_count) {
            ui_compose_paint_part(ui->project, ww, &fr->parts[ui->metasprite_edit.sel_part], cx, cy,
                                  ui->metasprite_edit.paint_color);
        }
    } else if (ui->metasprite_edit.dragging == 1 && ui->metasprite_edit.sel_part >= 0 &&
               ui->metasprite_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, UI_METASPRITE_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        fr->parts[ui->metasprite_edit.sel_part].dx = ui_compose_clamp_part(cx - ui->metasprite_edit.drag_off_x);
        fr->parts[ui->metasprite_edit.sel_part].dy = ui_compose_clamp_part(cy - ui->metasprite_edit.drag_off_y);
    }
}

void metasprite_modal_key(UiState *ui, SDL_Keycode sym) {
    R01EntityFrame *fr = &ui->metasprite_edit.draft.frame;
    R01EntityPart *pt;
    if (!ui || !ui->metasprite_edit.open) {
        return;
    }
    if (ui->text.field_id > 0) {
        ui_text_key(&ui->text, sym, SDL_GetModState());
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

void metasprite_modal_layout(const UiState *ui, MetaspriteModalLayout *lo) {
    enum {
        C_BANK_LAB = 1,
        C_BANK,
        C_NAME,
        C_ID,
        C_COMPOSE,
        C_PAL_LAB,
        C_PAL,
        C_FOOTER
    };
    static const UiPanelCell cells[] = {
        {C_BANK_LAB, 0, 0, 1, 1},
        {C_BANK, 0, 1, 1, 2},       /* rows 1-2: 16+112 = 128 */
        {C_NAME, 2, 0, 1, 1},
        {C_ID, 2, 1, 1, 1},
        {C_COMPOSE, 2, 2, 1, 2},    /* rows 2-3: 112+16 = 128 */
        {C_PAL_LAB, 2, 5, 1, 1},
        {C_PAL, 2, 6, 1, 1},
        {C_FOOTER, 0, 8, 3, 1},
    };
    /* Bank starts one row before compose; compose ends one row after bank. */
    static const int row_hs[] = {UI_BTN_H, UI_BTN_H, 112, UI_BTN_H, UI_UNIT, UI_BTN_H, UI_PAL_GRID_SIZE, UI_UNIT,
                                 UI_BTN_H};
    UiPanel panel;
    int pad = UI_UNIT;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int name_lab;
    int i;

    ui_panel_init(&panel, 3, 9, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, (int)(sizeof(cells) / sizeof(cells[0])));
    ui_panel_set_col_w(&panel, 0, UI_ENTITY_BANK_GRID);
    ui_panel_set_col_w(&panel, 1, pad);
    ui_panel_set_col_w(&panel, 2, UI_METASPRITE_COMPOSE);
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

    ui_panel_cell(&panel, C_BANK_LAB, &cx, &cy, &cw, &ch);
    lo->left_label_y = cy;
    lo->left_grid_x = cx;
    name_lab = label_width("Sprite bank");
    lo->left_dots_x = cx + name_lab + pad;
    lo->left_dots_y = cy + (UI_BTN_H - UI_DOT_SIZE) / 2;

    ui_panel_cell(&panel, C_BANK, &cx, &cy, &cw, &ch);
    lo->left_grid_y = cy;

    ui_panel_cell(&panel, C_NAME, &cx, &cy, &cw, &ch);
    lo->right_name_y = cy;
    name_lab = label_width("Name");
    lo->right_name_x = cx + name_lab + pad;
    lo->right_name_w = (cx + cw) - lo->right_name_x;
    lo->right_name_w = (lo->right_name_w / pad) * pad;
    if (lo->right_name_w < UI_UNIT * 8) {
        lo->right_name_w = UI_UNIT * 8;
    }

    ui_panel_cell(&panel, C_ID, &cx, &cy, &cw, &ch);
    lo->right_id_y = cy;
    lo->right_id_w = cw;

    ui_panel_cell(&panel, C_COMPOSE, &cx, &cy, &cw, &ch);
    lo->right_grid_x = cx;
    lo->right_grid_y = cy;

    ui_panel_cell(&panel, C_PAL_LAB, &cx, &cy, &cw, &ch);
    lo->pal_label_x = cx;
    lo->pal_label_y = cy;

    ui_panel_cell(&panel, C_PAL, &cx, &cy, &cw, &ch);
    lo->pal_x = cx;
    lo->pal_y = cy;

    ui_panel_cell(&panel, C_FOOTER, &cx, &cy, &cw, &ch);
    lo->btn_y = cy;
    lo->left_btn_x = content_x;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
#if UI_PANEL_DEBUG_GRID
    lo->dbg_panel = panel;
#endif
}

