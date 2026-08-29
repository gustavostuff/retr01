#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <string.h>

static R01EntityState *edit_state(UiState *ui) {
    return r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
}

static R01EntityFrame *edit_frame(UiState *ui) {
    return r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, ui->entity_edit.frame);
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

static int clamp_origin(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > R01_ENTITY_COMPOSE_PX) {
        return R01_ENTITY_COMPOSE_PX;
    }
    return v;
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

static void draw_meta_icon(UiState *ui, SDL_Renderer *r, const R01MetaspriteDef *ms, int dx, int dy) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int i;
    fill_rect(r, dx, dy, UI_SPRITE_ICON, UI_SPRITE_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!ms) {
        return;
    }
    for (i = 0; i < ms->frame.part_count; i++) {
        draw_part(ui, r, &ms->frame.parts[i], dx, dy, 1, 0);
    }
    (void)w;
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
    R01EntityFrame *fr;
    R01EntityPart *pt;
    const uint8_t *src;
    uint8_t tile[R01_TILE_BYTES];
    int lx, ly;
    int sel;
    fr = edit_frame(ui);
    sel = ui->entity_edit.sel_part;
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
    r01_tile_set_pixel(tile, lx, ly, (uint8_t)(ui->entity_edit.paint_color & 3));
    (void)r01_chr_write_spr_tile(w, pt->bank, pt->tile_id, tile);
}

static int modal_meta_rows(void) {
    return UI_ENTITY_LIST_H / UI_SPRITE_ROW_H;
}

static int modal_meta_list_hit(UiState *ui, int lx, int ly, int *out_idx) {
    EntityModalLayout lo;
    const R01World *w;
    int row, idx;
    entity_modal_layout(&lo);
    w = r01_project_active_world_const(ui->project);
    if (!w || w->metasprite_count < 1) {
        return 0;
    }
    if (lx < lo.left_list_x || lx >= lo.left_list_x + UI_ENTITY_BANK_GRID || ly < lo.left_list_y ||
        ly >= lo.left_list_y + lo.left_list_h) {
        return 0;
    }
    row = (ly - lo.left_list_y) / UI_SPRITE_ROW_H;
    idx = ui->entity_edit.meta_scroll + row;
    if (idx < 0 || idx >= w->metasprite_count) {
        return 0;
    }
    if (out_idx) {
        *out_idx = idx;
    }
    return 1;
}

void entity_edit_open_new(UiState *ui) {
    if (!ui) {
        return;
    }
    memset(&ui->entity_edit, 0, sizeof(ui->entity_edit));
    ui->entity_edit.open = 1;
    ui->entity_edit.is_new = 1;
    ui->entity_edit.type_idx = -1;
    r01_entity_type_init(&ui->entity_edit.draft);
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.paint_color = 1;
    ui->entity_edit.paint_pal = 0;
    ui->entity_edit.show_guides = 1;
    ui->entity_edit.meta_scroll = 0;
    ui->entity_edit.states_unlocked = 0;
    ui->entity_edit.name_focus = 0;
}

void entity_edit_open(UiState *ui, int type_idx) {
    R01World *w;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return;
    }
    memset(&ui->entity_edit, 0, sizeof(ui->entity_edit));
    ui->entity_edit.open = 1;
    ui->entity_edit.is_new = 0;
    ui->entity_edit.type_idx = type_idx;
    ui->entity_edit.draft = w->entities[type_idx];
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.paint_color = 1;
    ui->entity_edit.paint_pal = 0;
    ui->entity_edit.show_guides = 1;
    ui->entity_edit.meta_scroll = 0;
    ui->entity_edit.states_unlocked = 0;
    ui->entity_edit.name_focus = 0;
}

static void entity_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    if (ui->entity_edit.draft.state_count < 1) {
        ui->entity_edit.draft.state_count = 1;
    }
    if (ui->entity_edit.is_new || ui->entity_edit.type_idx < 0) {
        idx = r01_world_entity_add(w);
        if (idx < 0) {
            ui_toast(ui, "entity catalog full", 1);
            return;
        }
        w->entities[idx] = ui->entity_edit.draft;
        ui->entity_edit.type_idx = idx;
        ui->entity_edit.is_new = 0;
        ui_toast(ui, "entity created", 0);
    } else {
        if (ui->entity_edit.type_idx >= w->entity_count) {
            ui_toast(ui, "bad entity index", 1);
            return;
        }
        w->entities[ui->entity_edit.type_idx] = ui->entity_edit.draft;
        ui_toast(ui, "entity saved", 0);
    }
    ui->entity_edit.open = 0;
}

void draw_entity_modal(UiState *ui, SDL_Renderer *r) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityState *st;
    R01EntityFrame *fr;
    int i;
    int row = w ? w->default_pal_row : 0;
    char spr_label[16];
    const char *title = ui->entity_edit.is_new ? "Add entity" : "Edit entity";

    entity_modal_layout(&lo);
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

    font_draw(r, lo.left_list_x, lo.left_label_y + 4, "Metasprites", 230, 230, 230);
    fill_rect(r, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    if (!w || w->metasprite_count < 1) {
        font_draw_centered(r, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h, "empty", 160,
                           160, 170);
    } else {
        int vis = modal_meta_rows();
        int max_scroll = w->metasprite_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->entity_edit.meta_scroll > max_scroll) {
            ui->entity_edit.meta_scroll = max_scroll;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->entity_edit.meta_scroll + i;
            int y = lo.left_list_y + i * UI_SPRITE_ROW_H;
            const char *label;
            if (idx >= w->metasprite_count) {
                break;
            }
            if (point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_list_x, y, UI_ENTITY_BANK_GRID, UI_SPRITE_ROW_H)) {
                fill_rect(r, lo.left_list_x, y, UI_ENTITY_BANK_GRID, UI_SPRITE_ROW_H, UI_COL_PANEL_R,
                          UI_COL_PANEL_G, UI_COL_PANEL_B);
            }
            draw_meta_icon(ui, r, &w->metasprites[idx], lo.left_list_x + UI_UNIT,
                           y + (UI_SPRITE_ROW_H - UI_SPRITE_ICON) / 2);
            label = w->metasprites[idx].name[0] ? w->metasprites[idx].name : "meta";
            font_draw(r, lo.left_list_x + UI_UNIT + UI_SPRITE_ICON + 4, y + 4, label, 230, 230, 230);
        }
    }

    font_draw(r, lo.right_grid_x, lo.right_state_y + 4, "State", 230, 230, 230);
    draw_dot_strip(r, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                   ui->entity_edit.states_unlocked ? R01_ENTITY_STATES_MAX : 1);
    {
        R01EntityState *s0 = edit_state(ui);
        fill_rect(r, lo.right_name_x, lo.right_name_y, lo.right_name_w, UI_BTN_H, 240, 240, 240);
        if (ui->entity_edit.name_focus) {
            draw_rect(r, lo.right_name_x, lo.right_name_y, lo.right_name_w, UI_BTN_H, 45, 125, 70);
        }
        if (s0) {
            font_draw(r, lo.right_name_x + 2, lo.right_name_y + 4, s0->name[0] ? s0->name : "Idle", 20, 20, 20);
        }
    }

    font_draw(r, lo.right_grid_x, lo.right_frame_y + 4, "Frame", 230, 230, 230);
    draw_dot_strip(r, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, ui->entity_edit.frame,
                   R01_ENTITY_FRAMES_MAX);

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

    st = edit_state(ui);
    fr = edit_frame(ui);
    if (fr) {
        int sel = ui->entity_edit.sel_part;
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
    if (st && ui->entity_edit.show_guides) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        {
            SDL_Rect hb = {lo.right_grid_x + st->hitbox_x * 8, lo.right_grid_y + st->hitbox_y * 8,
                           st->hitbox_w * 8, st->hitbox_h * 8};
            SDL_RenderFillRect(r, &hb);
        }
        draw_ui_cross(r, lo.right_grid_x + st->origin_x * 8, lo.right_grid_y + st->origin_y * 8);
    }

    if (ui->entity_edit.dragging == 6 && w) {
        const R01MetaspriteDef *ms = r01_world_metasprite_const(w, ui->entity_edit.drag_meta);
        if (ms && ms->frame.part_count > 0) {
            int min_dx = ms->frame.parts[0].dx;
            int min_dy = ms->frame.parts[0].dy;
            int pi;
            for (pi = 1; pi < ms->frame.part_count; pi++) {
                if (ms->frame.parts[pi].dx < min_dx) {
                    min_dx = ms->frame.parts[pi].dx;
                }
                if (ms->frame.parts[pi].dy < min_dy) {
                    min_dy = ms->frame.parts[pi].dy;
                }
            }
            for (i = 0; i < ms->frame.part_count; i++) {
                const R01EntityPart *pt = &ms->frame.parts[i];
                R01EntityPart ghost = *pt;
                ghost.dx = (ui->mouse_x - ui->entity_edit.drag_off_x) / 8 + (pt->dx - min_dx);
                ghost.dy = (ui->mouse_y - ui->entity_edit.drag_off_y) / 8 + (pt->dy - min_dy);
                draw_part(ui, r, &ghost, lo.right_grid_x, lo.right_grid_y, 8, 0);
            }
        }
    }

    draw_checkbox_sprite(r, lo.guides_x, lo.guides_y + 4, ui->entity_edit.show_guides);
    font_draw(r, lo.guides_x + UI_CHECKBOX + UI_MODE_GAP, lo.guides_y + 4, "Origin/hitbox", 230, 230, 230);

    snprintf(spr_label, sizeof(spr_label), "SPR %d", row);
    font_draw(r, lo.pal_label_x, lo.pal_label_y + 4, spr_label, 230, 230, 230);
    draw_spr_palette_grid(r, ui->project, row, lo.pal_x, lo.pal_y, ui->entity_edit.paint_pal,
                          ui->entity_edit.paint_color);

    if (ui->entity_edit.dragging == 5) {
        draw_brush_preview(r, ui->project, row, ui->entity_edit.paint_pal, ui->entity_edit.paint_color, ui->mouse_x,
                           ui->mouse_y);
    }

    {
        int save_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_list_x, lo.btn_y, lo.save_w, UI_BTN_H);
        int cancel_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_list_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w,
                          UI_BTN_H);
        draw_button(r, lo.left_list_x, lo.btn_y, lo.save_w, "Save", 1, save_hover);
        draw_button(r, lo.left_list_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, "Cancel", 0, cancel_hover);
    }
}

int entity_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    const R01World *w;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);

    entity_modal_layout(&lo);
    st = edit_state(ui);
    fr = edit_frame(ui);
    w = r01_project_active_world_const(ui->project);

    if (!down) {
        if (!right && ui->entity_edit.dragging == 6 && fr && w &&
            point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
            int cx = (lx - lo.right_grid_x) / 8;
            int cy = (ly - lo.right_grid_y) / 8;
            const R01MetaspriteDef *ms = r01_world_metasprite_const(w, ui->entity_edit.drag_meta);
            if (ms) {
                if (r01_entity_frame_add_metasprite(fr, ms, cx, cy) != 0) {
                    ui_toast(ui, "frame part limit", 1);
                }
            }
        }
        ui->entity_edit.dragging = 0;
        return 1;
    }

    if (point_in_rect(lx, ly, lo.guides_x, lo.guides_y, UI_UNIT * 16, UI_BTN_H)) {
        ui->entity_edit.show_guides = !ui->entity_edit.show_guides;
        return 1;
    }
    if (spr_palette_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        return 1;
    }

    ui->entity_edit.name_focus = 0;
    if (point_in_rect(lx, ly, lo.right_name_x, lo.right_name_y, lo.right_name_w, UI_BTN_H)) {
        ui->entity_edit.name_focus = 1;
        return 1;
    }

    if (dot_strip_hit(lx, ly, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, &idx)) {
        int unlock = ui->entity_edit.states_unlocked ? R01_ENTITY_STATES_MAX : 1;
        if (idx < unlock) {
            ui->entity_edit.state = idx;
            if (ui->entity_edit.draft.state_count <= idx) {
                while (ui->entity_edit.draft.state_count <= idx &&
                       ui->entity_edit.draft.state_count < R01_ENTITY_STATES_MAX) {
                    r01_entity_state_init(&ui->entity_edit.draft.states[ui->entity_edit.draft.state_count],
                                          "State");
                    ui->entity_edit.draft.state_count++;
                }
            }
            ui->entity_edit.frame = 0;
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }
    if (dot_strip_hit(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, &idx)) {
        ui->entity_edit.frame = idx;
        (void)r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, idx);
        ui->entity_edit.sel_part = -1;
        return 1;
    }

    if (point_in_rect(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w, UI_BTN_H)) {
        entity_edit_save(ui);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.left_list_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H)) {
        ui->entity_edit.open = 0;
        return 1;
    }

    if (!right && modal_meta_list_hit(ui, lx, ly, &idx)) {
        const R01MetaspriteDef *ms = r01_world_metasprite_const(w, idx);
        int min_dx = 0, min_dy = 0, pi;
        if (ms && ms->frame.part_count > 0) {
            min_dx = ms->frame.parts[0].dx;
            min_dy = ms->frame.parts[0].dy;
            for (pi = 1; pi < ms->frame.part_count; pi++) {
                if (ms->frame.parts[pi].dx < min_dx) {
                    min_dx = ms->frame.parts[pi].dx;
                }
                if (ms->frame.parts[pi].dy < min_dy) {
                    min_dy = ms->frame.parts[pi].dy;
                }
            }
        }
        ui->entity_edit.dragging = 6;
        ui->entity_edit.drag_meta = idx;
        ui->entity_edit.drag_off_x = lx - min_dx * 8;
        ui->entity_edit.drag_off_y = ly - min_dy * 8;
        return 1;
    }

    if (st && point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        if (right) {
            idx = part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0) {
                if (idx != ui->entity_edit.sel_part) {
                    ui->entity_edit.sel_part = idx;
                    return 1;
                }
                paint_selected_part(ui, cx, cy);
                ui->entity_edit.dragging = 5;
            } else {
                ui->entity_edit.sel_part = -1;
            }
        } else if (ui->entity_edit.show_guides) {
            int ox = st->origin_x;
            int oy = st->origin_y;
            if (cx >= ox - 1 && cx <= ox + 1 && cy >= oy - 1 && cy <= oy + 1) {
                ui->entity_edit.dragging = 3;
                ui->entity_edit.drag_off_x = cx - ox;
                ui->entity_edit.drag_off_y = cy - oy;
                return 1;
            }
            if (cx >= st->hitbox_x && cx < st->hitbox_x + st->hitbox_w && cy >= st->hitbox_y &&
                cy < st->hitbox_y + st->hitbox_h) {
                ui->entity_edit.dragging = 2;
                ui->entity_edit.drag_off_x = cx - st->hitbox_x;
                ui->entity_edit.drag_off_y = cy - st->hitbox_y;
                return 1;
            }
        }
        if (fr) {
            idx = part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0) {
                ui->entity_edit.sel_part = idx;
                ui->entity_edit.dragging = 1;
                ui->entity_edit.drag_off_x = cx - fr->parts[idx].dx;
                ui->entity_edit.drag_off_y = cy - fr->parts[idx].dy;
                return 1;
            }
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }
    return 1;
}

void entity_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    if (!ui || !ui->entity_edit.open || !ui->entity_edit.dragging) {
        return;
    }
    entity_modal_layout(&lo);
    st = edit_state(ui);
    fr = edit_frame(ui);
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_RMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        paint_selected_part(ui, cx, cy);
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        fr->parts[ui->entity_edit.sel_part].dx = clamp_compose(cx - ui->entity_edit.drag_off_x);
        fr->parts[ui->entity_edit.sel_part].dy = clamp_compose(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 2 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        st->hitbox_x = clamp_compose(cx - ui->entity_edit.drag_off_x);
        st->hitbox_y = clamp_compose(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 3 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        st->origin_x = clamp_origin(cx - ui->entity_edit.drag_off_x);
        st->origin_y = clamp_origin(cy - ui->entity_edit.drag_off_y);
    }
}

void entity_modal_key(UiState *ui, SDL_Keycode sym) {
    R01EntityFrame *fr;
    R01EntityPart *pt;
    R01EntityState *st;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    st = edit_state(ui);
    if (ui->entity_edit.name_focus && st) {
        size_t len = strlen(st->name);
        if (sym == SDLK_BACKSPACE) {
            if (len > 0) {
                st->name[len - 1] = '\0';
            }
            return;
        }
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
            ui->entity_edit.name_focus = 0;
            return;
        }
        if (sym >= 32 && sym < 127 && len + 1 < R01_ENTITY_NAME_MAX) {
            st->name[len] = (char)sym;
            st->name[len + 1] = '\0';
            return;
        }
        return;
    }
    if (sym >= SDLK_1 && sym <= SDLK_4) {
        ui->entity_edit.paint_color = (int)(sym - SDLK_1);
        return;
    }
    fr = edit_frame(ui);
    if (!fr || ui->entity_edit.sel_part < 0 || ui->entity_edit.sel_part >= fr->part_count) {
        return;
    }
    pt = &fr->parts[ui->entity_edit.sel_part];
    if (sym == SDLK_h) {
        pt->flip_h = !pt->flip_h;
    } else if (sym == SDLK_v) {
        pt->flip_v = !pt->flip_v;
    } else if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
        r01_entity_frame_remove_part(fr, ui->entity_edit.sel_part);
        ui->entity_edit.sel_part = -1;
    }
}
