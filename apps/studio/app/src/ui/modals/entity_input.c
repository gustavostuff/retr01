#include "ui/modals/entity_edit_internal.h"
#include "ui/undo/undo_cmds.h"

#include "retr01_studio/entities.h"
#include "retr01_studio/project.h"

int entity_modal_wheel(UiState *ui, int lx, int ly, int wheel_y, int shift) {
    EntityModalLayout lo;
    const R01World *w;
    int row;
    int focus;
    int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
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
        int step = entity_edit_compose_scale(ui);
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        if (ctrl) {
            entity_edit_set_zoom(ui, &lo, ui->entity_edit.zoom + (wheel_y > 0 ? 1 : -1), lx, ly);
        } else if (shift) {
            /* Shift+wheel down (y<0) scrolls right. */
            entity_edit_view_pan(ui, (wheel_y > 0 ? -step : step) * 2, 0);
        } else {
            entity_edit_view_pan(ui, 0, (wheel_y > 0 ? -step : step) * 2);
        }
        return 1;
    }

    focus = ui_focus_get(ui);
    if (focus == UI_FOCUS_PALETTE) {
        ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                              ui->entity_edit.paint_color, wheel_y, shift);
        return 1;
    }
    if (focus == UI_FOCUS_WORKBENCH) {
        int step = entity_edit_compose_scale(ui);
        if (ctrl) {
            entity_edit_set_zoom(ui, &lo, ui->entity_edit.zoom + (wheel_y > 0 ? 1 : -1), lo.right_grid_x + UI_ENTITY_COMPOSE / 2,
                            lo.right_grid_y + UI_ENTITY_COMPOSE / 2);
        } else if (shift) {
            entity_edit_view_pan(ui, (wheel_y > 0 ? -step : step) * 2, 0);
        } else {
            entity_edit_view_pan(ui, 0, (wheel_y > 0 ? -step : step) * 2);
        }
        return 1;
    }
    return 0;
}
int entity_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);
    int middle = (button == SDL_BUTTON_MIDDLE);
    int cx, cy;

    entity_modal_layout(ui, &lo);
    st = entity_edit_state(ui);
    fr = entity_edit_frame(ui);

    if (!down) {
        if (ui->entity_edit.dragging == 1) {
            entity_edit_recompute_guides(ui);
        }
        if (ui->entity_edit.dragging == 5) {
            ui_undo_spr_paint_end(ui);
        }
        if (ui->entity_edit.dragging == 6 && right && !ui->entity_edit.pan_moved) {
            entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            menu_open_entity_compose(ui, lx, ly, cx, cy);
        }
        ui->entity_edit.dragging = 0;
        ui->entity_edit.pan_moved = 0;
        ui->entity_edit.pan_btn = 0;
        ui_text_mouse_up(&ui->text);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        ui_undo_spr_paint_end(ui);
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(&ui->text);
        return 1;
    }

    if (ui_multi_state_hit(lx, ly, lo.mode_x, lo.mode_y, lo.mode_w, 3, ui->entity_edit.tool, &idx)) {
        ui_text_blur(&ui->text);
        ui->entity_edit.tool = idx;
        ui->entity_edit.dragging = 0;
        return 1;
    }
    if (point_in_rect(lx, ly, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w, UI_BTN_H)) {
        ui_text_blur(&ui->text);
        if (fr && fr->part_count < R01_ENTITY_PARTS_MAX) {
            int wx = R01_ENTITY_COMPOSE_PX / 2;
            int wy = R01_ENTITY_COMPOSE_PX / 2;
            (void)entity_edit_add_sprite_at(ui, wx, wy);
        }
        return 1;
    }
    if (point_in_rect(lx, ly, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w, UI_BTN_H)) {
        ui_text_blur(&ui->text);
        if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
            R01EntityPart removed = fr->parts[ui->entity_edit.sel_part];
            int pidx = ui->entity_edit.sel_part;
            r01_entity_frame_remove_part(fr, pidx);
            ui->entity_edit.sel_part = -1;
            entity_edit_recompute_guides(ui);
            ui_undo_push_entity_part_remove(ui, ui->entity_edit.state, ui->entity_edit.frame, pidx, &removed);
        }
        return 1;
    }
    if (point_in_rect(lx, ly, lo.highlight_x, lo.highlight_y, lo.highlight_w, UI_BTN_H)) {
        ui_text_blur(&ui->text);
        ui->entity_edit.show_part_outlines = !ui->entity_edit.show_part_outlines;
        return 1;
    }
    if (ui_slider_discrete_hit(lx, ly, lo.brush_x, lo.brush_y, lo.brush_w, UI_BRUSH_SIZE_COUNT, &idx)) {
        ui_text_blur(&ui->text);
        ui->entity_edit.brush_size = idx + UI_BRUSH_SIZE_MIN;
        ui->entity_edit.dragging = 7;
        return 1;
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(&ui->text);
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
            entity_edit_apply_pal_to_part(ui, &fr->parts[ui->entity_edit.sel_part], pal);
        }
        return 1;
    }

    if (ui_text_mouse_down(&ui->text, lx, ly, lo.name_x, lo.name_y, lo.name_w, ui->entity_edit.draft.name,
                           R01_ENTITY_NAME_MAX, 1)) {
        return 1;
    }
    if (st && ui_text_mouse_down(&ui->text, lx, ly, lo.state_name_x, lo.state_name_y, lo.state_name_w, st->name,
                                 R01_ENTITY_NAME_MAX, 2)) {
        return 1;
    }
    ui_text_blur(&ui->text);

    if (ui_dot_strip_hit(lx, ly, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, &idx)) {
        if (ui->entity_edit.preview_playing) {
            return 1;
        }
        int unlock = entity_edit_state_unlock_count(ui);
        if (idx < unlock) {
            if (!r01_entity_ensure_state(&ui->entity_edit.draft, idx)) {
                return 1;
            }
            ui->entity_edit.state = idx;
            ui->entity_edit.frame = 0;
            ui->entity_edit.sel_part = -1;
            ui->entity_edit.preview_ctr = 0;
        }
        return 1;
    }
    if (ui_dot_strip_hit(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, &idx)) {
        if (ui->entity_edit.preview_playing) {
            return 1;
        }
        int unlock = entity_edit_frame_unlock_count(ui);
        if (idx < unlock) {
            ui->entity_edit.frame = idx;
            (void)r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, idx);
            ui->entity_edit.sel_part = -1;
            ui->entity_edit.preview_ctr = 0;
        }
        return 1;
    }

    if (point_in_rect(lx, ly, lo.play_x, lo.btn_y, lo.play_w, UI_BTN_H)) {
        ui_text_blur(&ui->text);
        if (ui->entity_edit.preview_playing) {
            entity_edit_preview_set(ui, 0);
        } else if (entity_edit_preview_can_play(ui)) {
            entity_edit_preview_set(ui, 1);
        }
        return 1;
    }
    if (ui_modal_save_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w)) {
        entity_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui_undo_spr_paint_end(ui);
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(&ui->text);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        if (ui->entity_edit.preview_playing) {
            return 1;
        }
        entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        if (ui->entity_edit.tool == UI_ENTITY_TOOL_PAINT && right) {
            R01World *ww = r01_project_active_world(ui->project);
            int col = 0;
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (ww && fr && idx >= 0 &&
                ui_compose_sample_part(ui->project, ww, &fr->parts[idx], cx, cy, &col)) {
                ui->entity_edit.sel_part = idx;
                ui->entity_edit.paint_color = col;
                ui->entity_edit.paint_pal = fr->parts[idx].pal & 3;
            }
            return 1;
        }
        if (middle || right) {
            ui->entity_edit.dragging = 6;
            ui->entity_edit.pan_moved = 0;
            ui->entity_edit.pan_btn = (int)button;
            ui->entity_edit.drag_off_x = lx;
            ui->entity_edit.drag_off_y = ly;
            return 1;
        }
        if (ui->entity_edit.tool == UI_ENTITY_TOOL_GUIDES && fr) {
            int corner = 0;
            if (entity_edit_origin_hit(ui, &lo, fr, lx, ly)) {
                ui->entity_edit.dragging = 2;
                ui->entity_edit.drag_off_x = cx - fr->origin_x;
                ui->entity_edit.drag_off_y = cy - fr->origin_y;
                return 1;
            }
            if (entity_edit_hitbox_corner_hit(ui, &lo, fr, lx, ly, &corner)) {
                /* Anchor = opposite corner in world px. */
                int ax = (corner == 1 || corner == 2) ? fr->hitbox_x : fr->hitbox_x + fr->hitbox_w;
                int ay = (corner == 2 || corner == 3) ? fr->hitbox_y : fr->hitbox_y + fr->hitbox_h;
                ui->entity_edit.dragging = 4;
                ui->entity_edit.drag_corner = corner;
                ui->entity_edit.drag_off_x = ax;
                ui->entity_edit.drag_off_y = ay;
                return 1;
            }
            if (entity_edit_hitbox_body_hit(ui, &lo, fr, lx, ly)) {
                ui->entity_edit.dragging = 3;
                ui->entity_edit.drag_off_x = cx - fr->hitbox_x;
                ui->entity_edit.drag_off_y = cy - fr->hitbox_y;
                return 1;
            }
            return 1;
        }
        if (ui->entity_edit.tool == UI_ENTITY_TOOL_PAINT) {
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0 && fr) {
                R01World *ww = r01_project_active_world(ui->project);
                if (ww) {
                    if (ui->entity_edit.dragging != 5) {
                        (void)ui_undo_spr_paint_begin(ui);
                    }
                    entity_edit_paint_at(ui, ww, fr, idx, cx, cy);
                }
                ui->entity_edit.dragging = 5;
            }
            return 1;
        }
        if (fr) {
            idx = ui_compose_part_at(fr, cx, cy, -1);
            if (idx >= 0) {
                idx = r01_entity_frame_bring_part_front(fr, idx);
                entity_edit_select_part(ui, fr, idx);
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
    R01EntityFrame *fr;
    int cx, cy;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    entity_modal_layout(ui, &lo);
    if (ui->text.drag && ui->text.field_id == 1) {
        ui_text_mouse_drag(&ui->text, lx, lo.name_x, lo.name_w);
        return;
    }
    if (ui->text.drag && ui->text.field_id == 2) {
        ui_text_mouse_drag(&ui->text, lx, lo.state_name_x, lo.state_name_w);
        return;
    }
    if (!ui->entity_edit.dragging) {
        return;
    }
    fr = entity_edit_frame(ui);
    if (ui->entity_edit.dragging == 7) {
        if ((buttons & SDL_BUTTON_LMASK) == 0) {
            return;
        }
        {
            int v = 0;
            int span = lo.brush_w - 8;
            int rel = lx - lo.brush_x - 4;
            if (span < 1) {
                span = 1;
            }
            if (rel < 0) {
                rel = 0;
            }
            if (rel > span) {
                rel = span;
            }
            v = (rel * (UI_BRUSH_SIZE_COUNT - 1) + span / 2) / span;
            if (v < 0) {
                v = 0;
            }
            if (v >= UI_BRUSH_SIZE_COUNT) {
                v = UI_BRUSH_SIZE_COUNT - 1;
            }
            ui->entity_edit.brush_size = v + UI_BRUSH_SIZE_MIN;
        }
        return;
    }
    if (ui->entity_edit.dragging == 6) {
        int held = 0;
        int dx, dy;
        if (ui->entity_edit.pan_btn == SDL_BUTTON_MIDDLE) {
            held = (buttons & SDL_BUTTON_MMASK) != 0;
        } else if (ui->entity_edit.pan_btn == SDL_BUTTON_RIGHT) {
            held = (buttons & SDL_BUTTON_RMASK) != 0;
        }
        if (!held) {
            return;
        }
        dx = lx - ui->entity_edit.drag_off_x;
        dy = ly - ui->entity_edit.drag_off_y;
        if (dx || dy) {
            if (dx * dx + dy * dy >= 9) {
                ui->entity_edit.pan_moved = 1;
            }
            entity_edit_view_pan(ui, -dx, -dy);
            ui->entity_edit.drag_off_x = lx;
            ui->entity_edit.drag_off_y = ly;
        }
        return;
    }
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_LMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        {
            R01World *ww = r01_project_active_world(ui->project);
            int idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (ww && fr && idx >= 0) {
                entity_edit_paint_at(ui, ww, fr, idx, cx, cy);
            }
        }
    } else if (ui->entity_edit.dragging == 2 && (buttons & SDL_BUTTON_LMASK)) {
        if (fr) {
            entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            fr->origin_x = ui_compose_clamp_origin(cx - ui->entity_edit.drag_off_x);
            fr->origin_y = ui_compose_clamp_origin(cy - ui->entity_edit.drag_off_y);
        }
    } else if (ui->entity_edit.dragging == 3 && (buttons & SDL_BUTTON_LMASK)) {
        if (fr) {
            int hx, hy, hw, hh;
            entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            hx = cx - ui->entity_edit.drag_off_x;
            hy = cy - ui->entity_edit.drag_off_y;
            hw = fr->hitbox_w;
            hh = fr->hitbox_h;
            ui_compose_clamp_hitbox(&hx, &hy, &hw, &hh);
            fr->hitbox_x = hx;
            fr->hitbox_y = hy;
            fr->hitbox_w = hw;
            fr->hitbox_h = hh;
        }
    } else if (ui->entity_edit.dragging == 4 && (buttons & SDL_BUTTON_LMASK)) {
        if (fr) {
            int ax = ui->entity_edit.drag_off_x;
            int ay = ui->entity_edit.drag_off_y;
            int x0, y0, x1, y1, hx, hy, hw, hh;
            entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            if (cx < 0) {
                cx = 0;
            }
            if (cy < 0) {
                cy = 0;
            }
            if (cx > R01_ENTITY_COMPOSE_PX) {
                cx = R01_ENTITY_COMPOSE_PX;
            }
            if (cy > R01_ENTITY_COMPOSE_PX) {
                cy = R01_ENTITY_COMPOSE_PX;
            }
            x0 = ax < cx ? ax : cx;
            y0 = ay < cy ? ay : cy;
            x1 = ax > cx ? ax : cx;
            y1 = ay > cy ? ay : cy;
            hx = x0;
            hy = y0;
            hw = x1 - x0;
            hh = y1 - y0;
            if (hw < 1) {
                hw = 1;
            }
            if (hh < 1) {
                hh = 1;
            }
            ui_compose_clamp_hitbox(&hx, &hy, &hw, &hh);
            fr->hitbox_x = hx;
            fr->hitbox_y = hy;
            fr->hitbox_w = hw;
            fr->hitbox_h = hh;
        }
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_edit_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
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
        ui_text_key(&ui->text, sym, SDL_GetModState());
        return;
    }
    if (sym == SDLK_SPACE) {
        ui->entity_edit.show_part_outlines = !ui->entity_edit.show_part_outlines;
        return;
    }
    st = entity_edit_state(ui);
    if (sym >= SDLK_1 && sym <= SDLK_4) {
        ui->entity_edit.paint_color = (int)(sym - SDLK_1);
        return;
    }
    fr = entity_edit_frame(ui);
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
            R01EntityPart removed = fr->parts[ui->entity_edit.sel_part];
            int pidx = ui->entity_edit.sel_part;
            r01_entity_frame_remove_part(fr, pidx);
            ui->entity_edit.sel_part = -1;
            entity_edit_recompute_guides(ui);
            ui_undo_push_entity_part_remove(ui, ui->entity_edit.state, ui->entity_edit.frame, pidx, &removed);
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
