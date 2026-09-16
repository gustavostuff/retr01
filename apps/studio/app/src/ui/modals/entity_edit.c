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

static int entity_compose_scale(const UiState *ui) {
    int z = ui ? ui->entity_edit.zoom : UI_ENTITY_ZOOM_MIN;
    if (z < UI_ENTITY_ZOOM_MIN) {
        z = UI_ENTITY_ZOOM_MIN;
    }
    if (z > UI_ENTITY_ZOOM_MAX) {
        z = UI_ENTITY_ZOOM_MAX;
    }
    return UI_ENTITY_COMPOSE_SCALE * z;
}

static void entity_screen_to_world(const UiState *ui, const EntityModalLayout *lo, int lx, int ly, int *wx,
                                   int *wy) {
    int sc = entity_compose_scale(ui);
    if (wx) {
        *wx = (lx - lo->right_grid_x + ui->entity_edit.view_x) / sc;
    }
    if (wy) {
        *wy = (ly - lo->right_grid_y + ui->entity_edit.view_y) / sc;
    }
}

static void entity_world_to_screen(const UiState *ui, const EntityModalLayout *lo, int wx, int wy, int *sx, int *sy) {
    int sc = entity_compose_scale(ui);
    if (sx) {
        *sx = lo->right_grid_x - ui->entity_edit.view_x + wx * sc;
    }
    if (sy) {
        *sy = lo->right_grid_y - ui->entity_edit.view_y + wy * sc;
    }
}

/* 1 if (lx,ly) hits the origin cross glyph. */
static int entity_origin_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                             int ly) {
    int sx, sy;
    if (!st) {
        return 0;
    }
    entity_world_to_screen(ui, lo, st->origin_x, st->origin_y, &sx, &sy);
    return point_in_rect(lx, ly, sx - UI_DOT_SIZE / 2, sy - UI_DOT_SIZE / 2, UI_DOT_SIZE, UI_DOT_SIZE);
}

/* 1 if near a hitbox corner; out_corner 0=NW 1=NE 2=SE 3=SW. */
static int entity_hitbox_corner_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                                    int ly, int *out_corner) {
    int sc = entity_compose_scale(ui);
    int grab = sc > UI_UNIT ? sc : UI_UNIT;
    int i;
    int cx[4], cy[4];
    if (!st || st->hitbox_w < 1 || st->hitbox_h < 1) {
        return 0;
    }
    entity_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y, &cx[0], &cy[0]);
    entity_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y, &cx[1], &cy[1]);
    entity_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y + st->hitbox_h, &cx[2], &cy[2]);
    entity_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y + st->hitbox_h, &cx[3], &cy[3]);
    for (i = 0; i < 4; i++) {
        if (point_in_rect(lx, ly, cx[i] - grab / 2, cy[i] - grab / 2, grab, grab)) {
            if (out_corner) {
                *out_corner = i;
            }
            return 1;
        }
    }
    return 0;
}

static int entity_hitbox_body_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                                  int ly) {
    int x0, y0, x1, y1;
    if (!st || st->hitbox_w < 1 || st->hitbox_h < 1) {
        return 0;
    }
    entity_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y, &x0, &y0);
    entity_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y + st->hitbox_h, &x1, &y1);
    return point_in_rect(lx, ly, x0, y0, x1 - x0, y1 - y0);
}

static void entity_view_max(const UiState *ui, int *out_max_x, int *out_max_y) {
    int sc = entity_compose_scale(ui);
    int full = R01_ENTITY_COMPOSE_PX * sc;
    int max_x = full - UI_ENTITY_COMPOSE;
    int max_y = full - UI_ENTITY_COMPOSE;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (out_max_x) {
        *out_max_x = max_x;
    }
    if (out_max_y) {
        *out_max_y = max_y;
    }
}

static void entity_view_clamp(UiState *ui) {
    int max_x, max_y;
    if (!ui) {
        return;
    }
    entity_view_max(ui, &max_x, &max_y);
    if (ui->entity_edit.view_x < 0) {
        ui->entity_edit.view_x = 0;
    }
    if (ui->entity_edit.view_y < 0) {
        ui->entity_edit.view_y = 0;
    }
    if (ui->entity_edit.view_x > max_x) {
        ui->entity_edit.view_x = max_x;
    }
    if (ui->entity_edit.view_y > max_y) {
        ui->entity_edit.view_y = max_y;
    }
}

static void entity_view_pan(UiState *ui, int dx, int dy) {
    if (!ui) {
        return;
    }
    ui->entity_edit.view_x += dx;
    ui->entity_edit.view_y += dy;
    entity_view_clamp(ui);
}

static void entity_set_zoom(UiState *ui, const EntityModalLayout *lo, int new_zoom, int focus_lx, int focus_ly) {
    int new_sc;
    int wx, wy;
    if (!ui || !lo) {
        return;
    }
    if (new_zoom < UI_ENTITY_ZOOM_MIN) {
        new_zoom = UI_ENTITY_ZOOM_MIN;
    }
    if (new_zoom > UI_ENTITY_ZOOM_MAX) {
        new_zoom = UI_ENTITY_ZOOM_MAX;
    }
    if (new_zoom == ui->entity_edit.zoom) {
        return;
    }
    entity_screen_to_world(ui, lo, focus_lx, focus_ly, &wx, &wy);
    ui->entity_edit.zoom = new_zoom;
    new_sc = entity_compose_scale(ui);
    /* Keep the world pixel under the cursor stable across zoom. */
    ui->entity_edit.view_x = wx * new_sc - (focus_lx - lo->right_grid_x);
    ui->entity_edit.view_y = wy * new_sc - (focus_ly - lo->right_grid_y);
    entity_view_clamp(ui);
}

static void entity_recompute_current_guides(UiState *ui) {
    R01EntityState *st = edit_state(ui);
    if (st) {
        r01_entity_state_recompute_guides(st);
    }
}

static void entity_sync_catalog_pal(R01World *w, const R01EntityPart *pt) {
    int i;
    if (!w || !pt) {
        return;
    }
    for (i = 0; i < w->sprite_count; i++) {
        if (w->sprites[i].bank == pt->bank && w->sprites[i].tile_id == pt->tile_id) {
            (void)r01_world_sprite_set_pal(w, i, pt->pal & 3);
            return;
        }
    }
}

static void entity_apply_pal_to_part(UiState *ui, R01EntityPart *pt, int pal) {
    R01World *w;
    if (!ui || !pt) {
        return;
    }
    pt->pal = pal & 3;
    w = r01_project_active_world(ui->project);
    entity_sync_catalog_pal(w, pt);
}

static void entity_select_part(UiState *ui, R01EntityFrame *fr, int idx) {
    if (!ui || !fr || idx < 0 || idx >= fr->part_count) {
        ui->entity_edit.sel_part = -1;
        return;
    }
    ui->entity_edit.sel_part = idx;
    ui->entity_edit.paint_pal = fr->parts[idx].pal & 3;
}

static void entity_paint_at(UiState *ui, R01World *w, R01EntityFrame *fr, int idx, int cx, int cy) {
    R01EntityPart *pt;
    if (!ui || !w || !fr || idx < 0 || idx >= fr->part_count) {
        return;
    }
    pt = &fr->parts[idx];
    entity_apply_pal_to_part(ui, pt, ui->entity_edit.paint_pal);
    ui->entity_edit.sel_part = idx;
    ui_undo_spr_paint_touch_tile(ui, pt->bank, pt->tile_id);
    (void)ui_compose_paint_brush(ui->project, w, pt, cx, cy, ui->entity_edit.paint_color,
                                 ui->entity_edit.brush_size);
}

int entity_edit_add_sprite_at(UiState *ui, int wx, int wy) {
    R01World *w;
    R01EntityFrame *fr;
    R01EntityPart part;
    int bank;
    int tile_id;
    int idx;
    int cat;

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
    cat = r01_world_sprite_add(w, bank, tile_id, ui->entity_edit.paint_pal);
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
    ui_undo_push_entity_part_add(ui, ui->entity_edit.state, ui->entity_edit.frame, idx, &part, cat);
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
    ui->entity_edit.brush_size = UI_BRUSH_SIZE_MIN;
    ui->entity_edit.tool = UI_ENTITY_TOOL_SELECT;
    ui->entity_edit.show_part_outlines = 0;
    ui->entity_edit.zoom = UI_ENTITY_ZOOM_MIN;
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
    ui->entity_edit.brush_size = UI_BRUSH_SIZE_MIN;
    ui->entity_edit.tool = UI_ENTITY_TOOL_SELECT;
    ui->entity_edit.show_part_outlines = 0;
    ui->entity_edit.zoom = UI_ENTITY_ZOOM_MIN;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
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
    ui_undo_spr_paint_end(ui);
    ui->entity_edit.open = 0;
    ui_focus_clear(ui);
    ui_text_blur(ui);
}

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
        int step = entity_compose_scale(ui);
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        if (ctrl) {
            entity_set_zoom(ui, &lo, ui->entity_edit.zoom + (wheel_y > 0 ? 1 : -1), lx, ly);
        } else if (shift) {
            /* Shift+wheel down (y<0) scrolls right. */
            entity_view_pan(ui, (wheel_y > 0 ? -step : step) * 2, 0);
        } else {
            entity_view_pan(ui, 0, (wheel_y > 0 ? -step : step) * 2);
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
        int step = entity_compose_scale(ui);
        if (ctrl) {
            entity_set_zoom(ui, &lo, ui->entity_edit.zoom + (wheel_y > 0 ? 1 : -1), lo.right_grid_x + UI_ENTITY_COMPOSE / 2,
                            lo.right_grid_y + UI_ENTITY_COMPOSE / 2);
        } else if (shift) {
            entity_view_pan(ui, (wheel_y > 0 ? -step : step) * 2, 0);
        } else {
            entity_view_pan(ui, 0, (wheel_y > 0 ? -step : step) * 2);
        }
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
    int sc = entity_compose_scale(ui);
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
    ui_modal_scrim(r, ui);
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, title);
#if UI_PANEL_DEBUG_GRID
    ui_panel_debug_draw(r, &lo.dbg_panel);
#endif

    {
        const char *ename = ui->entity_edit.draft.name[0] ? ui->entity_edit.draft.name : "Entity";
        font_draw(r, lo.name_x - label_width("Name"), lo.name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(ui, r, lo.name_x, lo.name_y, lo.name_w, ename, 1);
    }

    {
        R01EntityState *s0 = edit_state(ui);
        const char *sname = (s0 && s0->name[0]) ? s0->name : "Idle";
        font_draw(r, lo.state_name_x - label_width("State name"), lo.state_name_y + 4, "State name", 230,
                  230, 230);
        ui_text_draw(ui, r, lo.state_name_x, lo.state_name_y, lo.state_name_w, sname, 2);
    }

    {
        R01EntityFrame *fr0 = edit_frame(ui);
        int can_add = fr0 && fr0->part_count < R01_ENTITY_PARTS_MAX;
        int can_rem = fr0 && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr0->part_count;
        int add_hover = can_add && point_in_rect(ui->mouse_x, ui->mouse_y, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w,
                                                 UI_BTN_H);
        int rem_hover = can_rem && point_in_rect(ui->mouse_x, ui->mouse_y, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w,
                                                 UI_BTN_H);
        ui_button_draw_ex(r, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w, "Add", 1, add_hover, can_add);
        ui_button_draw_ex(r, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w, "Remove", 0, rem_hover, can_rem);
        ui_checkbox_draw(r, lo.highlight_x, lo.highlight_y + 4, ui->entity_edit.show_part_outlines);
        font_draw(r, lo.highlight_x + UI_CHECKBOX + UI_MODE_GAP, lo.highlight_y + 4, "Highlight", 230, 230,
                  230);
        font_draw(r, lo.brush_lab_x, lo.brush_lab_y + 4, "Brush", 230, 230, 230);
        ui_slider_discrete_draw(r, lo.brush_x, lo.brush_y, lo.brush_w, ui->entity_edit.brush_size - UI_BRUSH_SIZE_MIN,
                                UI_BRUSH_SIZE_COUNT);
    }

    font_draw(r, lo.state_dots_x - label_width("State"), lo.state_y + 4, "State", 230, 230, 230);
    ui_dot_strip_draw(r, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                      state_unlock_count(ui));

    font_draw(r, lo.frame_dots_x - label_width("Frame"), lo.frame_y + 4, "Frame", 230, 230, 230);
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

    fill_rect(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    st = edit_state(ui);
    fr = edit_frame(ui);
    ui_clip_push(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, &clip);
    ui_compose_draw_grid(r, ox, oy, full, sc);
    ui_compose_draw_frame(r, ui->project, w, fr, ox, oy, sc, ui->entity_edit.sel_part,
                          ui->entity_edit.show_part_outlines, part_alpha);
    if (st && guides_mode) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        {
            SDL_Rect hb = {ox + st->hitbox_x * sc, oy + st->hitbox_y * sc, st->hitbox_w * sc, st->hitbox_h * sc};
            SDL_RenderFillRect(r, &hb);
        }
        draw_ui_cross(r, ox + st->origin_x * sc, oy + st->origin_y * sc);
    }
    if (ui->entity_edit.tool == UI_ENTITY_TOOL_PAINT && !ui->menu.open &&
        point_in_rect(ui->mouse_x, ui->mouse_y, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE,
                      UI_ENTITY_COMPOSE)) {
        int cx, cy, idx;
        entity_screen_to_world(ui, &lo, ui->mouse_x, ui->mouse_y, &cx, &cy);
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
    st = edit_state(ui);
    fr = edit_frame(ui);

    if (!down) {
        if (ui->entity_edit.dragging == 1) {
            entity_recompute_current_guides(ui);
        }
        if (ui->entity_edit.dragging == 5) {
            ui_undo_spr_paint_end(ui);
        }
        if (ui->entity_edit.dragging == 6 && right && !ui->entity_edit.pan_moved) {
            entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            menu_open_entity_compose(ui, lx, ly, cx, cy);
        }
        ui->entity_edit.dragging = 0;
        ui->entity_edit.pan_moved = 0;
        ui->entity_edit.pan_btn = 0;
        ui_text_mouse_up(ui);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        ui_undo_spr_paint_end(ui);
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (ui_multi_state_hit(lx, ly, lo.mode_x, lo.mode_y, lo.mode_w, 3, ui->entity_edit.tool, &idx)) {
        ui_text_blur(ui);
        ui->entity_edit.tool = idx;
        ui->entity_edit.dragging = 0;
        return 1;
    }
    if (point_in_rect(lx, ly, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w, UI_BTN_H)) {
        ui_text_blur(ui);
        if (fr && fr->part_count < R01_ENTITY_PARTS_MAX) {
            int wx = R01_ENTITY_COMPOSE_PX / 2;
            int wy = R01_ENTITY_COMPOSE_PX / 2;
            (void)entity_edit_add_sprite_at(ui, wx, wy);
        }
        return 1;
    }
    if (point_in_rect(lx, ly, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w, UI_BTN_H)) {
        ui_text_blur(ui);
        if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
            R01EntityPart removed = fr->parts[ui->entity_edit.sel_part];
            int pidx = ui->entity_edit.sel_part;
            r01_entity_frame_remove_part(fr, pidx);
            ui->entity_edit.sel_part = -1;
            entity_recompute_current_guides(ui);
            ui_undo_push_entity_part_remove(ui, ui->entity_edit.state, ui->entity_edit.frame, pidx, &removed);
        }
        return 1;
    }
    if (point_in_rect(lx, ly, lo.highlight_x, lo.highlight_y, lo.highlight_w, UI_BTN_H)) {
        ui_text_blur(ui);
        ui->entity_edit.show_part_outlines = !ui->entity_edit.show_part_outlines;
        return 1;
    }
    if (ui_slider_discrete_hit(lx, ly, lo.brush_x, lo.brush_y, lo.brush_w, UI_BRUSH_SIZE_COUNT, &idx)) {
        ui_text_blur(ui);
        ui->entity_edit.brush_size = idx + UI_BRUSH_SIZE_MIN;
        ui->entity_edit.dragging = 7;
        return 1;
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(ui);
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
            entity_apply_pal_to_part(ui, &fr->parts[ui->entity_edit.sel_part], pal);
        }
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
        ui_undo_spr_paint_end(ui);
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        if (ui->entity_edit.tool == UI_ENTITY_TOOL_PAINT && right) {
            R01World *ww = r01_project_active_world(ui->project);
            int col = 0;
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (ww && fr && idx >= 0 && ui_compose_sample_part(ww, &fr->parts[idx], cx, cy, &col)) {
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
        if (ui->entity_edit.tool == UI_ENTITY_TOOL_GUIDES && st) {
            int corner = 0;
            if (entity_origin_hit(ui, &lo, st, lx, ly)) {
                ui->entity_edit.dragging = 2;
                ui->entity_edit.drag_off_x = cx - st->origin_x;
                ui->entity_edit.drag_off_y = cy - st->origin_y;
                return 1;
            }
            if (entity_hitbox_corner_hit(ui, &lo, st, lx, ly, &corner)) {
                /* Anchor = opposite corner in world px. */
                int ax = (corner == 1 || corner == 2) ? st->hitbox_x : st->hitbox_x + st->hitbox_w;
                int ay = (corner == 2 || corner == 3) ? st->hitbox_y : st->hitbox_y + st->hitbox_h;
                ui->entity_edit.dragging = 4;
                ui->entity_edit.drag_corner = corner;
                ui->entity_edit.drag_off_x = ax;
                ui->entity_edit.drag_off_y = ay;
                return 1;
            }
            if (entity_hitbox_body_hit(ui, &lo, st, lx, ly)) {
                ui->entity_edit.dragging = 3;
                ui->entity_edit.drag_off_x = cx - st->hitbox_x;
                ui->entity_edit.drag_off_y = cy - st->hitbox_y;
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
                    entity_paint_at(ui, ww, fr, idx, cx, cy);
                }
                ui->entity_edit.dragging = 5;
            }
            return 1;
        }
        if (fr) {
            idx = ui_compose_part_at(fr, cx, cy, -1);
            if (idx >= 0) {
                idx = r01_entity_frame_bring_part_front(fr, idx);
                entity_select_part(ui, fr, idx);
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
            entity_view_pan(ui, -dx, -dy);
            ui->entity_edit.drag_off_x = lx;
            ui->entity_edit.drag_off_y = ly;
        }
        return;
    }
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_LMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        {
            R01World *ww = r01_project_active_world(ui->project);
            int idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (ww && fr && idx >= 0) {
                entity_paint_at(ui, ww, fr, idx, cx, cy);
            }
        }
    } else if (ui->entity_edit.dragging == 2 && (buttons & SDL_BUTTON_LMASK)) {
        R01EntityState *st = edit_state(ui);
        if (st) {
            entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            st->origin_x = ui_compose_clamp_origin(cx - ui->entity_edit.drag_off_x);
            st->origin_y = ui_compose_clamp_origin(cy - ui->entity_edit.drag_off_y);
        }
    } else if (ui->entity_edit.dragging == 3 && (buttons & SDL_BUTTON_LMASK)) {
        R01EntityState *st = edit_state(ui);
        if (st) {
            int hx, hy, hw, hh;
            entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
            hx = cx - ui->entity_edit.drag_off_x;
            hy = cy - ui->entity_edit.drag_off_y;
            hw = st->hitbox_w;
            hh = st->hitbox_h;
            ui_compose_clamp_hitbox(&hx, &hy, &hw, &hh);
            st->hitbox_x = hx;
            st->hitbox_y = hy;
            st->hitbox_w = hw;
            st->hitbox_h = hh;
        }
    } else if (ui->entity_edit.dragging == 4 && (buttons & SDL_BUTTON_LMASK)) {
        R01EntityState *st = edit_state(ui);
        if (st) {
            int ax = ui->entity_edit.drag_off_x;
            int ay = ui->entity_edit.drag_off_y;
            int x0, y0, x1, y1, hx, hy, hw, hh;
            entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
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
            st->hitbox_x = hx;
            st->hitbox_y = hy;
            st->hitbox_w = hw;
            st->hitbox_h = hh;
        }
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
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
            R01EntityPart removed = fr->parts[ui->entity_edit.sel_part];
            int pidx = ui->entity_edit.sel_part;
            r01_entity_frame_remove_part(fr, pidx);
            ui->entity_edit.sel_part = -1;
            entity_recompute_current_guides(ui);
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
