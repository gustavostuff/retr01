#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/project.h"

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

static void draw_meta_icon(UiState *ui, SDL_Renderer *r, const R01MetaspriteDef *ms, int dx, int dy) {
    const R01World *w = r01_project_active_world_const(ui->project);
    ui_compose_draw_frame_icon(r, ui->project, w, ms ? &ms->frame : NULL, dx, dy, UI_PREVIEW_ICON);
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
    ui->entity_edit.meta_scroll = 0;
    ui_text_blur(ui);
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
    ui_text_blur(ui);
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
    ui_modal_scrim(r);
    ui_modal_panel(r, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H, title);

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
            draw_meta_icon(ui, r, &w->metasprites[idx], lo.left_list_x,
                           y + (UI_SPRITE_ROW_H - UI_PREVIEW_ICON) / 2);
            label = r01_metasprite_display_name(&w->metasprites[idx]);
            font_draw_clipped(r, lo.left_list_x + UI_PREVIEW_ICON + 2, y + 4,
                              lo.left_list_x + UI_PREVIEW_ICON + 2, y,
                              UI_ENTITY_BANK_GRID - UI_PREVIEW_ICON - 2, UI_SPRITE_ROW_H, label, 230, 230, 230);
        }
    }

    {
        const char *ename = ui->entity_edit.draft.name[0] ? ui->entity_edit.draft.name : "Entity";
        font_draw(r, lo.right_grid_x, lo.right_ent_name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(ui, r, lo.right_ent_name_x, lo.right_ent_name_y, lo.right_ent_name_w, ename, 1);
    }

    font_draw(r, lo.right_grid_x, lo.right_state_y + 4, "State", 230, 230, 230);
    ui_dot_strip_draw(r, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                      state_unlock_count(ui));
    {
        R01EntityState *s0 = edit_state(ui);
        const char *sname = (s0 && s0->name[0]) ? s0->name : "Idle";
        ui_text_draw(ui, r, lo.right_name_x, lo.right_name_y, lo.right_name_w, sname, 2);
    }

    font_draw(r, lo.right_grid_x, lo.right_frame_y + 4, "Frame", 230, 230, 230);
    ui_dot_strip_draw(r, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, ui->entity_edit.frame,
                      frame_unlock_count(ui));
    {
        char fid[R01_ID_MAX];
        int wi = ui->project ? ui->project->active_world : 0;
        r01_entity_frame_id(fid, sizeof(fid), wi, &ui->entity_edit.draft, ui->entity_edit.state,
                            ui->entity_edit.frame);
        font_draw_clipped(r, lo.right_grid_x, lo.right_id_y + 4, lo.right_grid_x, lo.right_id_y,
                          lo.mx + UI_ENTITY_MODAL_W - UI_UNIT * 2 - lo.right_grid_x, UI_BTN_H, fid, 160, 160,
                          170);
    }

    ui_compose_draw_grid(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE);
    st = edit_state(ui);
    fr = edit_frame(ui);
    ui_compose_draw_frame(r, ui->project, w, fr, lo.right_grid_x, lo.right_grid_y, 8, ui->entity_edit.sel_part);

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
            int gx = ui->mouse_x - ui->entity_edit.drag_off_x;
            int gy = ui->mouse_y - ui->entity_edit.drag_off_y;
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
                ghost.dx = pt->dx - min_dx;
                ghost.dy = pt->dy - min_dy;
                /* Follow the cursor in screen pixels (1x), not compose-grid space. */
                ui_compose_draw_part(r, ui->project, w, &ghost, gx, gy, 1, 0);
            }
        }
    }

    ui_checkbox_draw(r, lo.guides_x, lo.guides_y + 4, ui->entity_edit.show_guides);
    font_draw(r, lo.guides_x + UI_CHECKBOX + UI_MODE_GAP, lo.guides_y + 4, "Origin/hitbox", 230, 230, 230);

    snprintf(spr_label, sizeof(spr_label), "SPR %d", row);
    font_draw(r, lo.pal_label_x, lo.pal_label_y + 4, spr_label, 230, 230, 230);
    ui_palette_grid_draw(r, ui->project, row, lo.pal_x, lo.pal_y, ui->entity_edit.paint_pal,
                         ui->entity_edit.paint_color, UI_PAL_PLANE_SPR);

    if (ui->entity_edit.dragging == 5) {
        draw_brush_preview(r, ui->project, row, ui->entity_edit.paint_pal, ui->entity_edit.paint_color, ui->mouse_x,
                           ui->mouse_y);
    }

    ui_modal_save_cancel(r, lo.left_list_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
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
            /* Ghost AABB min corner is at (mouse - drag_off); map that into compose cells. */
            int cx = (lx - ui->entity_edit.drag_off_x - lo.right_grid_x) / 8;
            int cy = (ly - ui->entity_edit.drag_off_y - lo.right_grid_y) / 8;
            const R01MetaspriteDef *ms = r01_world_metasprite_const(w, ui->entity_edit.drag_meta);
            if (ms) {
                if (r01_entity_frame_add_metasprite(fr, ms, cx, cy) != 0) {
                    ui_toast(ui, "frame part limit", 1);
                }
            }
        }
        ui->entity_edit.dragging = 0;
        ui_text_mouse_up(ui);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, UI_ENTITY_MODAL_W, UI_ENTITY_MODAL_H)) {
        ui->entity_edit.open = 0;
        ui_text_blur(ui);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.guides_x, lo.guides_y, UI_UNIT * 16, UI_BTN_H)) {
        ui_text_blur(ui);
        ui->entity_edit.show_guides = !ui->entity_edit.show_guides;
        return 1;
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(ui);
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        return 1;
    }

    if (ui_text_mouse_down(ui, lx, ly, lo.right_ent_name_x, lo.right_ent_name_y, lo.right_ent_name_w,
                           ui->entity_edit.draft.name, R01_ENTITY_NAME_MAX, 1)) {
        return 1;
    }
    if (st && ui_text_mouse_down(ui, lx, ly, lo.right_name_x, lo.right_name_y, lo.right_name_w, st->name,
                                 R01_ENTITY_NAME_MAX, 2)) {
        return 1;
    }
    ui_text_blur(ui);

    if (ui_dot_strip_hit(lx, ly, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, &idx)) {
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

    if (ui_modal_save_hit(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w)) {
        entity_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui->entity_edit.open = 0;
        ui_text_blur(ui);
        return 1;
    }

    if (!right && modal_meta_list_hit(ui, lx, ly, &idx)) {
        ui->entity_edit.dragging = 6;
        ui->entity_edit.drag_meta = idx;
        /* Ghost min-corner tracks the cursor (small grab offset). */
        ui->entity_edit.drag_off_x = 4;
        ui->entity_edit.drag_off_y = 4;
        return 1;
    }

    if (st && point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        if (right) {
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0) {
                if (idx != ui->entity_edit.sel_part) {
                    ui->entity_edit.sel_part = idx;
                    return 1;
                }
                {
                    R01World *ww = r01_project_active_world(ui->project);
                    if (ww && fr) {
                        ui_compose_paint_part(ui->project, ww, &fr->parts[idx], cx, cy,
                                              ui->entity_edit.paint_color);
                    }
                }
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
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
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
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    entity_modal_layout(&lo);
    if (ui->text.drag && ui->text.field_id == 1) {
        ui_text_mouse_drag(ui, lx, lo.right_ent_name_x, lo.right_ent_name_w);
        return;
    }
    if (ui->text.drag && ui->text.field_id == 2) {
        ui_text_mouse_drag(ui, lx, lo.right_name_x, lo.right_name_w);
        return;
    }
    if (!ui->entity_edit.dragging) {
        return;
    }
    st = edit_state(ui);
    fr = edit_frame(ui);
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_RMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        R01World *ww = r01_project_active_world(ui->project);
        if (ww && fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
            ui_compose_paint_part(ui->project, ww, &fr->parts[ui->entity_edit.sel_part], cx, cy,
                                  ui->entity_edit.paint_color);
        }
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        fr->parts[ui->entity_edit.sel_part].dx = ui_compose_clamp_part(cx - ui->entity_edit.drag_off_x);
        fr->parts[ui->entity_edit.sel_part].dy = ui_compose_clamp_part(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 2 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        st->hitbox_x = ui_compose_clamp_part(cx - ui->entity_edit.drag_off_x);
        st->hitbox_y = ui_compose_clamp_part(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 3 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        int cx = (lx - lo.right_grid_x) / 8;
        int cy = (ly - lo.right_grid_y) / 8;
        st->origin_x = ui_compose_clamp_origin(cx - ui->entity_edit.drag_off_x);
        st->origin_y = ui_compose_clamp_origin(cy - ui->entity_edit.drag_off_y);
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
            return;
        }
        return;
    }
    /* No part selected: Delete trims empty last frame, then empty last state. */
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
