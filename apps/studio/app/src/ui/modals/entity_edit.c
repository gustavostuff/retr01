#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/undo/undo_cmds.h"
#include "font/font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdio.h>
#include <string.h>

static R01EntityState *edit_state(UiState *ui) {
    return r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
}

static R01EntityFrame *edit_frame(UiState *ui) {
    return r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, ui->entity_edit.frame);
}

static int state_unlock_count(const UiState *ui) {
    int n = ui->entity_edit.draft.state_count + 1;
    if (n > R01_ENTITY_STATES_MAX) {
        n = R01_ENTITY_STATES_MAX;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static int frame_unlock_count(UiState *ui) {
    R01EntityState *st = r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
    int n = 1;
    if (st) {
        n = st->frame_count + 1;
    }
    if (n > R01_ENTITY_FRAMES_MAX) {
        n = R01_ENTITY_FRAMES_MAX;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static void entity_screen_to_world(const EntityModalLayout *lo, int lx, int ly, int *wx, int *wy) {
    int sc = UI_ENTITY_COMPOSE_SCALE;
    if (wx) {
        *wx = (lx - lo->right_grid_x) / sc;
    }
    if (wy) {
        *wy = (ly - lo->right_grid_y) / sc;
    }
}

static void entity_recompute_current_guides(UiState *ui) {
    R01EntityState *st = edit_state(ui);
    if (st) {
        r01_entity_state_recompute_guides(st);
    }
}

int entity_edit_add_sprite_at(UiState *ui, int wx, int wy) {
    R01World *w;
    R01EntityFrame *fr;
    R01EntityPart part;
    int bank;
    int tile_id;
    int idx;

    if (!ui || !ui->entity_edit.open) {
        return -1;
    }
    fr = edit_frame(ui);
    if (!fr) {
        return -1;
    }
    if (fr->part_count >= R01_ENTITY_PARTS_MAX) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return -1;
    }
    bank = r01_chr_find_spr_bank_space(w);
    if (bank < 0) {
        ui_toast(ui, "sprite banks full", 1);
        return -1;
    }
    tile_id = r01_chr_alloc_spr_tile(w, bank);
    if (tile_id < 0) {
        ui_toast(ui, "sprite banks full", 1);
        return -1;
    }
    (void)r01_world_sprite_add(w, bank, tile_id, ui->entity_edit.paint_pal);
    memset(&part, 0, sizeof(part));
    part.bank = bank;
    part.tile_id = tile_id;
    part.pal = ui->entity_edit.paint_pal & 3;
    part.dx = ui_compose_clamp_part(wx - 4);
    part.dy = ui_compose_clamp_part(wy - 4);
    idx = r01_entity_frame_add_part(fr, &part);
    if (idx < 0) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    ui->entity_edit.sel_part = idx;
    entity_recompute_current_guides(ui);
    return idx;
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
    ui->entity_edit.paint_mode = 0;
    ui->entity_edit.show_part_outlines = 0;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(ui);
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
    ui->entity_edit.paint_mode = 0;
    ui->entity_edit.show_part_outlines = 0;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(ui);
}

static void entity_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    int si;
    if (!w) {
        return;
    }
    if (ui->entity_edit.draft.state_count < 1) {
        ui->entity_edit.draft.state_count = 1;
    }
    for (si = 0; si < ui->entity_edit.draft.state_count; si++) {
        r01_entity_state_recompute_guides(&ui->entity_edit.draft.states[si]);
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
        ui_undo_push_entity_add(ui, idx);
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
    ui_focus_clear(ui);
    ui_text_blur(ui);
}

int entity_modal_wheel(UiState *ui, int lx, int ly, int wheel_y, int shift) {
    EntityModalLayout lo;
    const R01World *w;
    int row;
    int focus;
    if (!ui || !ui->entity_edit.open || wheel_y == 0) {
        return 0;
    }
    entity_modal_layout(ui, &lo);
    w = r01_project_active_world_const(ui->project);
    row = w ? w->default_pal_row : 0;

    if (point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE)) {
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                              ui->entity_edit.paint_color, wheel_y, shift);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        return 1;
    }

    focus = ui_focus_get(ui);
    if (focus == UI_FOCUS_PALETTE) {
        ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                              ui->entity_edit.paint_color, wheel_y, shift);
        return 1;
    }
    return 0;
}

void draw_entity_modal(UiState *ui, SDL_Renderer *r) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityState *st;
    R01EntityFrame *fr;
    int row = w ? w->default_pal_row : 0;
    int sc = UI_ENTITY_COMPOSE_SCALE;
    UiClipStack clip;
    const char *title = ui->entity_edit.is_new ? "Add entity" : "Edit entity";

    entity_modal_layout(ui, &lo);
    ui_modal_scrim(r, ui);
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, title);

    {
        const char *ename = ui->entity_edit.draft.name[0] ? ui->entity_edit.draft.name : "Entity";
        font_draw(r, lo.mx + UI_UNIT, lo.name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(ui, r, lo.name_x, lo.name_y, lo.name_w, ename, 1);
    }

    {
        R01EntityState *s0 = edit_state(ui);
        const char *sname = (s0 && s0->name[0]) ? s0->name : "Idle";
        font_draw(r, lo.mx + UI_UNIT, lo.state_name_y + 4, "State name", 230, 230, 230);
        ui_text_draw(ui, r, lo.state_name_x, lo.state_name_y, lo.state_name_w, sname, 2);
    }

    font_draw(r, lo.mx + UI_UNIT, lo.state_y + 4, "State", 230, 230, 230);
    ui_dot_strip_draw(r, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                      state_unlock_count(ui));

    {
        int frame_lab_x = lo.frame_dots_x - label_width("Frame");
        if (frame_lab_x < lo.mx + UI_UNIT) {
            frame_lab_x = lo.mx + UI_UNIT;
        }
        font_draw(r, frame_lab_x, lo.frame_y + 4, "Frame", 230, 230, 230);
    }
    ui_dot_strip_draw(r, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, ui->entity_edit.frame,
                      frame_unlock_count(ui));

    {
        char fid[R01_ID_MAX];
        int wi = ui->project ? ui->project->active_world : 0;
        r01_entity_frame_id(fid, sizeof(fid), wi, &ui->entity_edit.draft, ui->entity_edit.state,
                            ui->entity_edit.frame);
        font_draw_clipped(r, lo.frame_id_x, lo.frame_id_y + 4, lo.frame_id_x, lo.frame_id_y, lo.frame_id_w,
                          UI_BTN_H, fid, 160, 160, 170);
    }

    ui_palette_grid_draw(r, ui->project, row, lo.pal_x, lo.pal_y, ui->entity_edit.paint_pal,
                         ui->entity_edit.paint_color, UI_PAL_PLANE_SPR);

    ui_compose_draw_grid(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, sc);
    st = edit_state(ui);
    fr = edit_frame(ui);
    ui_clip_push(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, &clip);
    ui_compose_draw_frame(r, ui->project, w, fr, lo.right_grid_x, lo.right_grid_y, sc, ui->entity_edit.sel_part,
                          ui->entity_edit.show_part_outlines);
    if (st && ui->entity_edit.show_guides) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        {
            SDL_Rect hb = {lo.right_grid_x + st->hitbox_x * sc, lo.right_grid_y + st->hitbox_y * sc,
                           st->hitbox_w * sc, st->hitbox_h * sc};
            SDL_RenderFillRect(r, &hb);
        }
        draw_ui_cross(r, lo.right_grid_x + st->origin_x * sc, lo.right_grid_y + st->origin_y * sc);
    }
    ui_clip_pop(r, &clip);

    ui_checkbox_draw(r, lo.guides_x, lo.guides_y + 4, ui->entity_edit.show_guides);
    font_draw(r, lo.guides_x + UI_CHECKBOX + UI_MODE_GAP, lo.guides_y + 4, "Origin/hitbox", 230, 230, 230);
    ui_checkbox_draw(r, lo.paint_x, lo.paint_y + 4, ui->entity_edit.paint_mode);
    font_draw(r, lo.paint_x + UI_CHECKBOX + UI_MODE_GAP, lo.paint_y + 4, "Paint", 230, 230, 230);

    if (ui->entity_edit.dragging == 5) {
        draw_brush_preview(r, ui->project, row, ui->entity_edit.paint_pal, ui->entity_edit.paint_color, ui->mouse_x,
                           ui->mouse_y);
    }

    ui_modal_save_cancel(r, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
}

int entity_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);
    int cx, cy;

    entity_modal_layout(ui, &lo);
    st = edit_state(ui);
    fr = edit_frame(ui);

    if (!down) {
        if (ui->entity_edit.dragging == 1) {
            entity_recompute_current_guides(ui);
        }
        ui->entity_edit.dragging = 0;
        ui_text_mouse_up(ui);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.guides_x, lo.guides_y, lo.paint_x - lo.guides_x, UI_BTN_H)) {
        ui_text_blur(ui);
        ui->entity_edit.show_guides = !ui->entity_edit.show_guides;
        return 1;
    }
    {
        int paint_lab_w = UI_CHECKBOX + UI_MODE_GAP + label_width("Paint");
        if (point_in_rect(lx, ly, lo.paint_x, lo.paint_y, paint_lab_w, UI_BTN_H)) {
            ui_text_blur(ui);
            ui->entity_edit.paint_mode = !ui->entity_edit.paint_mode;
            ui->entity_edit.dragging = 0;
            return 1;
        }
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(ui);
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        return 1;
    }

    if (ui_text_mouse_down(ui, lx, ly, lo.name_x, lo.name_y, lo.name_w, ui->entity_edit.draft.name,
                           R01_ENTITY_NAME_MAX, 1)) {
        return 1;
    }
    if (st && ui_text_mouse_down(ui, lx, ly, lo.state_name_x, lo.state_name_y, lo.state_name_w, st->name,
                                 R01_ENTITY_NAME_MAX, 2)) {
        return 1;
    }
    ui_text_blur(ui);

    if (ui_dot_strip_hit(lx, ly, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, &idx)) {
        int unlock = state_unlock_count(ui);
        if (idx < unlock) {
            if (!r01_entity_ensure_state(&ui->entity_edit.draft, idx)) {
                return 1;
            }
            ui->entity_edit.state = idx;
            ui->entity_edit.frame = 0;
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }
    if (ui_dot_strip_hit(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, &idx)) {
        int unlock = frame_unlock_count(ui);
        if (idx < unlock) {
            ui->entity_edit.frame = idx;
            (void)r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, idx);
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }

    if (ui_modal_save_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w)) {
        entity_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        entity_screen_to_world(&lo, lx, ly, &cx, &cy);
        if (right) {
            menu_open_entity_compose(ui, lx, ly, cx, cy);
            return 1;
        }
        if (ui->entity_edit.paint_mode) {
            idx = ui_compose_part_at(fr, cx, cy, -1);
            if (idx >= 0 && fr) {
                R01World *ww = r01_project_active_world(ui->project);
                if (ww) {
                    ui_compose_paint_part(ui->project, ww, &fr->parts[idx], cx, cy, ui->entity_edit.paint_color);
                }
                ui->entity_edit.sel_part = idx;
                ui->entity_edit.dragging = 5;
            }
            return 1;
        }
        if (fr) {
            idx = ui_compose_part_at(fr, cx, cy, -1);
            if (idx >= 0) {
                idx = r01_entity_frame_bring_part_front(fr, idx);
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
    (void)st;
    return 1;
}

void entity_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons) {
    EntityModalLayout lo;
    R01EntityFrame *fr;
    int cx, cy;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    entity_modal_layout(ui, &lo);
    if (ui->text.drag && ui->text.field_id == 1) {
        ui_text_mouse_drag(ui, lx, lo.name_x, lo.name_w);
        return;
    }
    if (ui->text.drag && ui->text.field_id == 2) {
        ui_text_mouse_drag(ui, lx, lo.state_name_x, lo.state_name_w);
        return;
    }
    if (!ui->entity_edit.dragging) {
        return;
    }
    fr = edit_frame(ui);
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_LMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(&lo, lx, ly, &cx, &cy);
        {
            R01World *ww = r01_project_active_world(ui->project);
            int idx = ui_compose_part_at(fr, cx, cy, -1);
            if (ww && fr && idx >= 0) {
                ui_compose_paint_part(ui->project, ww, &fr->parts[idx], cx, cy, ui->entity_edit.paint_color);
                ui->entity_edit.sel_part = idx;
            }
        }
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(&lo, lx, ly, &cx, &cy);
        fr->parts[ui->entity_edit.sel_part].dx = ui_compose_clamp_part(cx - ui->entity_edit.drag_off_x);
        fr->parts[ui->entity_edit.sel_part].dy = ui_compose_clamp_part(cy - ui->entity_edit.drag_off_y);
    }
}

void entity_modal_key(UiState *ui, SDL_Keycode sym) {
    R01EntityFrame *fr;
    R01EntityPart *pt;
    R01EntityState *st;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    if (ui->text.field_id > 0) {
        ui_text_key(ui, sym, SDL_GetModState());
        return;
    }
    if (sym == SDLK_SPACE) {
        ui->entity_edit.show_part_outlines = !ui->entity_edit.show_part_outlines;
        return;
    }
    st = edit_state(ui);
    if (sym >= SDLK_1 && sym <= SDLK_4) {
        ui->entity_edit.paint_color = (int)(sym - SDLK_1);
        return;
    }
    fr = edit_frame(ui);
    if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
        pt = &fr->parts[ui->entity_edit.sel_part];
        if (sym == SDLK_h) {
            pt->flip_h = !pt->flip_h;
            return;
        }
        if (sym == SDLK_v) {
            pt->flip_v = !pt->flip_v;
            return;
        }
        if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
            r01_entity_frame_remove_part(fr, ui->entity_edit.sel_part);
            ui->entity_edit.sel_part = -1;
            entity_recompute_current_guides(ui);
            return;
        }
        return;
    }
    if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
        R01EntityType *e = &ui->entity_edit.draft;
        if (st && ui->entity_edit.frame == st->frame_count - 1 && st->frame_count > 1) {
            if (r01_entity_trim_last_frame(e, ui->entity_edit.state)) {
                ui->entity_edit.frame = st->frame_count - 1;
                if (ui->entity_edit.frame < 0) {
                    ui->entity_edit.frame = 0;
                }
                return;
            }
        }
        if (ui->entity_edit.state == e->state_count - 1 && e->state_count > 1) {
            if (r01_entity_trim_last_state(e)) {
                ui->entity_edit.state = e->state_count - 1;
                ui->entity_edit.frame = 0;
                return;
            }
        }
    }
}
