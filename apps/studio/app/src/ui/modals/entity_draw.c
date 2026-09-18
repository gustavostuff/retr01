#include "ui/modals/entity_edit_internal.h"
#include "font/font.h"

#include "retr01_studio/entities.h"
#include "retr01_studio/project.h"

#include <stdio.h>

void draw_entity_modal(UiState *ui, SDL_Renderer *r) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityFrame *fr;
    R01EntityState *st;
    int row = w ? w->default_pal_row : 0;
    int sc = entity_edit_compose_scale(ui);
    int ox;
    int oy;
    int full = R01_ENTITY_COMPOSE_PX * sc;
    UiClipStack clip;
    static const char *const mode_labels[] = {"Sprite select", "Sprite paint", "Origin/hitbox"};
    const char *title = ui->entity_edit.is_new ? "Add entity" : "Edit entity";
    int guides_mode = ui->entity_edit.tool == UI_ENTITY_TOOL_GUIDES;
    Uint8 part_alpha = guides_mode ? 128 : 255;

    entity_modal_layout(ui, &lo);
    ox = lo.right_grid_x - ui->entity_edit.view_x;
    oy = lo.right_grid_y - ui->entity_edit.view_y;
    ui_modal_scrim(r, ui_logic_w(ui), ui_logic_h(ui));
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, title);
#if UI_PANEL_DEBUG_GRID
    ui_panel_debug_draw(r, &lo.dbg_panel);
#endif

    {
        const char *ename = ui->entity_edit.draft.name[0] ? ui->entity_edit.draft.name : "Entity";
        font_draw(r, lo.name_x - label_width("Name"), lo.name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(&ui->text, r, lo.name_x, lo.name_y, lo.name_w, ename, 1);
    }

    {
        R01EntityState *s0 = entity_edit_state(ui);
        const char *sname = (s0 && s0->name[0]) ? s0->name : "Idle";
        font_draw(r, lo.state_name_x - label_width("State name"), lo.state_name_y + 4, "State name", 230,
                  230, 230);
        ui_text_draw(&ui->text, r, lo.state_name_x, lo.state_name_y, lo.state_name_w, sname, 2);
    }

    {
        R01EntityFrame *fr0 = entity_edit_frame(ui);
        int can_add = fr0 && fr0->part_count < R01_ENTITY_PARTS_MAX;
        int can_rem = fr0 && fr0->part_count > 0 && ui->entity_edit.sel_mask != 0;
        int add_hover = can_add && point_in_rect(ui->mouse_x, ui->mouse_y, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w,
                                                 UI_BTN_H);
        int rem_hover = can_rem && point_in_rect(ui->mouse_x, ui->mouse_y, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w,
                                                 UI_BTN_H);
        ui_button_draw_ex(r, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w, "Add", 1, add_hover, can_add);
        ui_button_draw_ex(r, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w, "Remove", 0, rem_hover, can_rem);
        ui_checkbox_draw(r, lo.highlight_x, lo.highlight_y + 4, ui->entity_edit.show_part_outlines);
        font_draw(r, lo.highlight_x + UI_TOGGLE_W, lo.highlight_y + 4, "Highlight", 230, 230,
                  230);
        font_draw(r, lo.brush_lab_x, lo.brush_lab_y + 4, "Brush", 230, 230, 230);
        ui_slider_discrete_draw(r, lo.brush_x, lo.brush_y, lo.brush_w, ui->entity_edit.brush_size - UI_BRUSH_SIZE_MIN,
                                UI_BRUSH_SIZE_COUNT);
    }

    font_draw(r, lo.state_dots_x - label_width("State"), lo.state_y + 4, "State", 230, 230, 230);
    ui_dot_strip_draw(r, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                      entity_edit_state_unlock_count(ui));

    {
        R01EntityFrame *frd = entity_edit_frame(ui);
        int d = (frd && frd->delay > 0) ? frd->delay : 1;
        char lab[24];
        snprintf(lab, sizeof(lab), "Frame %df", d);
        font_draw(r, lo.frame_dots_x - label_width(lab), lo.frame_y + 4, lab, 230, 230, 230);
    }
    ui_dot_strip_draw(r, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, ui->entity_edit.frame,
                      entity_edit_frame_unlock_count(ui));

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

    fill_rect(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    fr = entity_edit_frame(ui);
    st = entity_edit_state(ui);
    ui_clip_push(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, &clip);
    ui_compose_draw_grid(r, ox, oy, full, sc);
    if (fr && ui->entity_edit.preview_playing) {
        int pin = R01_ENTITY_COMPOSE_PX / 2;
        ox += (pin - fr->origin_x) * sc;
        oy += (pin - fr->origin_y) * sc;
    }
    ui_compose_draw_frame(r, ui->project, w, fr, ox, oy, sc,
                          ui->entity_edit.preview_playing ? 0u : ui->entity_edit.sel_mask,
                          ui->entity_edit.show_part_outlines, part_alpha);
    if (ui->entity_edit.dragging == 8 && !ui->entity_edit.preview_playing) {
        int cx, cy, x0, y0, x1, y1, rw, rh;
        entity_edit_screen_to_world(ui, &lo, ui->mouse_x, ui->mouse_y, &cx, &cy);
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
        x0 = ui->entity_edit.drag_off_x < cx ? ui->entity_edit.drag_off_x : cx;
        y0 = ui->entity_edit.drag_off_y < cy ? ui->entity_edit.drag_off_y : cy;
        x1 = ui->entity_edit.drag_off_x > cx ? ui->entity_edit.drag_off_x : cx;
        y1 = ui->entity_edit.drag_off_y > cy ? ui->entity_edit.drag_off_y : cy;
        rw = (x1 - x0) * sc;
        rh = (y1 - y0) * sc;
        if (rw < 1) {
            rw = 1;
        }
        if (rh < 1) {
            rh = 1;
        }
        draw_marching_ants(r, ox + x0 * sc, oy + y0 * sc, rw, rh);
    }
    if (guides_mode) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        if (st) {
            SDL_Rect hb = {ox + st->hitbox_x * sc, oy + st->hitbox_y * sc, st->hitbox_w * sc, st->hitbox_h * sc};
            SDL_RenderFillRect(r, &hb);
        }
        if (fr) {
            draw_ui_cross(r, ox + fr->origin_x * sc, oy + fr->origin_y * sc);
        }
    }
    if (ui->entity_edit.tool == UI_ENTITY_TOOL_PAINT && !ui->menu.open && !ui->entity_edit.preview_playing &&
        point_in_rect(ui->mouse_x, ui->mouse_y, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE,
                      UI_ENTITY_COMPOSE)) {
        int cx, cy, idx;
        entity_edit_screen_to_world(ui, &lo, ui->mouse_x, ui->mouse_y, &cx, &cy);
        idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
        if (idx >= 0) {
            int bw, bh, box, boy, x, y;
            const uint8_t *bits;
            ui_compose_brush_stamp(ui->entity_edit.brush_size, &bw, &bh, &bits);
            box = (bw - 1) / 2;
            boy = (bh - 1) / 2;
            for (y = 0; y < bh; y++) {
                for (x = 0; x < bw; x++) {
                    int px, py;
                    if (!bits || !bits[y * bw + x]) {
                        continue;
                    }
                    px = cx - box + x;
                    py = cy - boy + y;
                    if (px < fr->parts[idx].dx || px >= fr->parts[idx].dx + 8 || py < fr->parts[idx].dy ||
                        py >= fr->parts[idx].dy + 8) {
                        continue;
                    }
                    draw_paint_pixel_preview(r, ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                                             ui->entity_edit.paint_color, ox + px * sc, oy + py * sc, sc);
                }
            }
        }
    }
    ui_clip_pop(r, &clip);

    font_draw(r, lo.guides_x, lo.guides_y + 4, "Mode", 230, 230, 230);
    ui_multi_state_draw(r, lo.mode_x, lo.mode_y, lo.mode_w, mode_labels, 3, ui->entity_edit.tool, ui->mouse_x,
                        ui->mouse_y);

    ui_modal_save_cancel(r, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
    {
        int can_play = ui->entity_edit.preview_playing || entity_edit_preview_can_play(ui);
        const char *play_lab = ui->entity_edit.preview_playing ? "Stop" : "Play";
        int play_hover = can_play && point_in_rect(ui->mouse_x, ui->mouse_y, lo.play_x, lo.btn_y, lo.play_w, UI_BTN_H);
        ui_button_draw_ex(r, lo.play_x, lo.btn_y, lo.play_w, play_lab, 1, play_hover, can_play);
    }
}
