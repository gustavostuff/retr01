#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
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

void entity_edit_open_new(UiState *ui) {
    if (!ui) {
        return;
    }
    memset(&ui->entity_edit, 0, sizeof(ui->entity_edit));
    ui->entity_edit.open = 1;
    ui->entity_edit.is_new = 1;
    ui->entity_edit.type_idx = -1;
    r01_entity_type_init(&ui->entity_edit.draft);
    ui->entity_edit.bank = 0;
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.drag_mode = UI_DRAG_SPRITES;
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.states_unlocked = 0; /* only state 0 */
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
    ui->entity_edit.bank = 0;
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.drag_mode = UI_DRAG_SPRITES;
    ui->entity_edit.sel_part = -1;
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

static void draw_bank_tile(UiState *ui, SDL_Renderer *r, int bank, int tile_id, int dx, int dy, int scale) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const uint8_t *tile;
    int row = w ? w->default_pal_row : 0;
    int pal = 0;
    int sy, sx;
    if (!w) {
        return;
    }
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
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
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, dx + sx * scale, dy + sy * scale, scale, scale, cr, cg, cb);
        }
    }
}

static void draw_part(UiState *ui, SDL_Renderer *r, const R01EntityPart *pt, int ox, int oy, int scale,
                      int selected) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const uint8_t *raw;
    uint8_t oriented[R01_TILE_BYTES];
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

void draw_entity_modal(UiState *ui, SDL_Renderer *r) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityState *st;
    R01EntityFrame *fr;
    int tx, ty, i;
    const char *title = ui->entity_edit.is_new ? "Add entity" : "Edit entity";

    entity_modal_layout(&lo);
    fill_rect(r, 0, 0, UI_LOGIC_W, UI_LOGIC_H, 0, 0, 0);
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        {
            SDL_Rect full = {0, 0, UI_LOGIC_W, UI_LOGIC_H};
            SDL_RenderFillRect(r, &full);
        }
    }
    fill_rect(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B);
    draw_rect(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    font_draw_centered(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_BTN_H, title, 240, 240, 240);

    font_draw(r, lo.left_grid_x, lo.left_label_y + 4, "Sprite bank", 230, 230, 230);
    draw_dot_strip(r, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, ui->entity_edit.bank, R01_SPR_BANKS);

    fill_rect(r, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    for (ty = 0; ty < 16; ty++) {
        for (tx = 0; tx < 16; tx++) {
            int tid = ty * 16 + tx;
            draw_bank_tile(ui, r, ui->entity_edit.bank, tid, lo.left_grid_x + tx * 8, lo.left_grid_y + ty * 8, 1);
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
    /* 16x16 pixel grid guides */
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
        for (i = 0; i < fr->part_count; i++) {
            draw_part(ui, r, &fr->parts[i], lo.right_grid_x, lo.right_grid_y, 8, i == ui->entity_edit.sel_part);
        }
    }
    if (st) {
        /* hitbox: red translucent 8x8 in compose space */
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        {
            SDL_Rect hb = {lo.right_grid_x + st->hitbox_x * 8, lo.right_grid_y + st->hitbox_y * 8,
                           st->hitbox_w * 8, st->hitbox_h * 8};
            SDL_RenderFillRect(r, &hb);
        }
        draw_ui_cross(r, lo.right_grid_x + st->origin_x * 8, lo.right_grid_y + st->origin_y * 8);
    }

    if (ui->entity_edit.dragging == 4) {
        draw_bank_tile(ui, r, ui->entity_edit.bank, ui->entity_edit.drag_tile,
                       ui->mouse_x - ui->entity_edit.drag_off_x, ui->mouse_y - ui->entity_edit.drag_off_y, 1);
    }

    {
        int ry = lo.radio_y;
        int sel0 = ui->entity_edit.drag_mode == UI_DRAG_SPRITES;
        int sel1 = ui->entity_edit.drag_mode == UI_DRAG_HITBOX;
        draw_radio_sprite(r, lo.radio_x, ry + 4, sel0);
        font_draw(r, lo.radio_x + UI_MODE_RADIO + UI_MODE_GAP, ry + 4, "Drag sprites", 230, 230, 230);
        draw_radio_sprite(r, lo.radio_x, ry + UI_BTN_H + 4, sel1);
        font_draw(r, lo.radio_x + UI_MODE_RADIO + UI_MODE_GAP, ry + UI_BTN_H + 4, "Drag hitbox/x,y", 230, 230,
                  230);
        (void)w;
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

static int part_at(const R01EntityFrame *fr, int px, int py) {
    int i;
    if (!fr) {
        return -1;
    }
    for (i = fr->part_count - 1; i >= 0; i--) {
        const R01EntityPart *pt = &fr->parts[i];
        if (px >= pt->dx && px < pt->dx + 8 && py >= pt->dy && py < pt->dy + 8) {
            return i;
        }
    }
    return -1;
}

int entity_modal_handle(UiState *ui, int lx, int ly, int down) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    int idx;
    entity_modal_layout(&lo);
    st = edit_state(ui);
    fr = edit_frame(ui);

    if (!down) {
        if (ui->entity_edit.dragging == 4 && fr &&
            point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
            R01EntityPart part;
            int cx = (lx - lo.right_grid_x) / 8;
            int cy = (ly - lo.right_grid_y) / 8;
            memset(&part, 0, sizeof(part));
            part.bank = ui->entity_edit.bank;
            part.tile_id = ui->entity_edit.drag_tile;
            part.pal = 0;
            /* Prefer catalog default pal if this tile is catalogued. */
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
            idx = r01_entity_frame_add_part(fr, &part);
            if (idx >= 0) {
                ui->entity_edit.sel_part = idx;
            } else {
                ui_toast(ui, "frame part limit", 1);
            }
        }
        ui->entity_edit.dragging = 0;
        return 1;
    }

    ui->entity_edit.name_focus = 0;
    if (point_in_rect(lx, ly, lo.right_name_x, lo.right_name_y, lo.right_name_w, UI_BTN_H)) {
        ui->entity_edit.name_focus = 1;
        return 1;
    }

    if (dot_strip_hit(lx, ly, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N, &idx)) {
        ui->entity_edit.bank = idx;
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

    if (point_in_rect(lx, ly, lo.radio_x, lo.radio_y, UI_UNIT * 20, UI_BTN_H)) {
        ui->entity_edit.drag_mode = UI_DRAG_SPRITES;
        return 1;
    }
    if (point_in_rect(lx, ly, lo.radio_x, lo.radio_y + UI_BTN_H, UI_UNIT * 20, UI_BTN_H)) {
        ui->entity_edit.drag_mode = UI_DRAG_HITBOX;
        return 1;
    }

    if (point_in_rect(lx, ly, lo.left_grid_x, lo.btn_y, lo.save_w, UI_BTN_H)) {
        entity_edit_save(ui);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H)) {
        ui->entity_edit.open = 0;
        return 1;
    }

    if (point_in_rect(lx, ly, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID)) {
        int txx = (lx - lo.left_grid_x) / 8;
        int tyy = (ly - lo.left_grid_y) / 8;
        ui->entity_edit.dragging = 4;
        ui->entity_edit.drag_tile = tyy * 16 + txx;
        ui->entity_edit.drag_off_x = lx - (lo.left_grid_x + txx * 8);
        ui->entity_edit.drag_off_y = ly - (lo.left_grid_y + tyy * 8);
        ui->entity_edit.drag_mode = UI_DRAG_SPRITES;
        return 1;
    }

    if (st && point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        if (ui->entity_edit.drag_mode == UI_DRAG_HITBOX) {
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
        } else if (fr) {
            idx = part_at(fr, cx, cy);
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

void entity_modal_drag(UiState *ui, int lx, int ly) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    if (!ui || !ui->entity_edit.open || !ui->entity_edit.dragging) {
        return;
    }
    entity_modal_layout(&lo);
    st = edit_state(ui);
    fr = edit_frame(ui);
    if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
        ui->entity_edit.sel_part < fr->part_count &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        fr->parts[ui->entity_edit.sel_part].dx = clamp_compose(cx - ui->entity_edit.drag_off_x);
        fr->parts[ui->entity_edit.sel_part].dy = clamp_compose(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 2 && st &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        st->hitbox_x = clamp_compose(cx - ui->entity_edit.drag_off_x);
        st->hitbox_y = clamp_compose(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 3 && st &&
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
    fr = edit_frame(ui);
    if (!fr || ui->entity_edit.sel_part < 0 || ui->entity_edit.sel_part >= fr->part_count) {
        return;
    }
    pt = &fr->parts[ui->entity_edit.sel_part];
    if (sym == SDLK_h) {
        pt->flip_h = !pt->flip_h;
    } else if (sym == SDLK_v) {
        pt->flip_v = !pt->flip_v;
    } else if (sym >= SDLK_1 && sym <= SDLK_4) {
        pt->pal = (int)(sym - SDLK_1);
    } else if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
        r01_entity_frame_remove_part(fr, ui->entity_edit.sel_part);
        ui->entity_edit.sel_part = -1;
    }
}
