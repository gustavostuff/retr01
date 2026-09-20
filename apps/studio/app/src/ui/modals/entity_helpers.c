#include "ui/modals/entity_edit_internal.h"
#include "ui/undo/undo_cmds.h"

#include "retr01_studio/entities.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <string.h>

R01EntityState *entity_edit_state(UiState *ui) {
    return r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
}

R01EntityFrame *entity_edit_frame(UiState *ui) {
    return r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, ui->entity_edit.frame);
}

int entity_edit_state_unlock_count(const UiState *ui) {
    int n = ui->entity_edit.draft.state_count + 1;
    if (n > R01_ENTITY_STATES_MAX) {
        n = R01_ENTITY_STATES_MAX;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

int entity_edit_frame_unlock_count(UiState *ui) {
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

int entity_edit_compose_scale(const UiState *ui) {
    int z = ui ? ui->entity_edit.zoom : UI_ENTITY_ZOOM_MIN;
    if (z < UI_ENTITY_ZOOM_MIN) {
        z = UI_ENTITY_ZOOM_MIN;
    }
    if (z > UI_ENTITY_ZOOM_MAX) {
        z = UI_ENTITY_ZOOM_MAX;
    }
    return UI_ENTITY_COMPOSE_SCALE * z;
}

void entity_edit_screen_to_world(const UiState *ui, const EntityModalLayout *lo, int lx, int ly, int *wx,
                                   int *wy) {
    int sc = entity_edit_compose_scale(ui);
    if (wx) {
        *wx = (lx - lo->right_grid_x + ui->entity_edit.view_x) / sc;
    }
    if (wy) {
        *wy = (ly - lo->right_grid_y + ui->entity_edit.view_y) / sc;
    }
}

void entity_edit_world_to_screen(const UiState *ui, const EntityModalLayout *lo, int wx, int wy, int *sx, int *sy) {
    int sc = entity_edit_compose_scale(ui);
    if (sx) {
        *sx = lo->right_grid_x - ui->entity_edit.view_x + wx * sc;
    }
    if (sy) {
        *sy = lo->right_grid_y - ui->entity_edit.view_y + wy * sc;
    }
}

/* 1 if (lx,ly) hits the origin cross glyph. */
int entity_edit_origin_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx,
                             int ly) {
    int sx, sy;
    if (!fr) {
        return 0;
    }
    entity_edit_world_to_screen(ui, lo, fr->origin_x, fr->origin_y, &sx, &sy);
    return point_in_rect(lx, ly, sx - UI_DOT_SIZE / 2, sy - UI_DOT_SIZE / 2, UI_DOT_SIZE, UI_DOT_SIZE);
}

/* 1 if near a hitbox corner; out_corner 0=NW 1=NE 2=SE 3=SW. */
int entity_edit_hitbox_corner_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                                    int ly, int *out_corner) {
    int handle = 0;
    if (!entity_edit_hitbox_handle_hit(ui, lo, st, lx, ly, &handle) || handle > UI_ENTITY_HB_SW) {
        return 0;
    }
    if (out_corner) {
        *out_corner = handle;
    }
    return 1;
}

int entity_edit_hitbox_handle_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                                  int ly, int *out_handle) {
    int sc = entity_edit_compose_scale(ui);
    int grab = sc > UI_UNIT ? sc : UI_UNIT;
    int i;
    int cx[4], cy[4];
    int inset;
    int x0, y0, x1, y1, ew, eh;
    if (!st || st->hitbox_w < 1 || st->hitbox_h < 1) {
        return 0;
    }
    entity_edit_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y, &cx[0], &cy[0]);
    entity_edit_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y, &cx[1], &cy[1]);
    entity_edit_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y + st->hitbox_h, &cx[2], &cy[2]);
    entity_edit_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y + st->hitbox_h, &cx[3], &cy[3]);
    for (i = 0; i < 4; i++) {
        if (point_in_rect(lx, ly, cx[i] - grab / 2, cy[i] - grab / 2, grab, grab)) {
            if (out_handle) {
                *out_handle = i;
            }
            return 1;
        }
    }
    x0 = cx[0];
    y0 = cy[0];
    x1 = cx[2];
    y1 = cy[2];
    if (x1 < x0) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    inset = grab / 2;
    ew = x1 - x0 - grab;
    eh = y1 - y0 - grab;
    if (ew > 0 && point_in_rect(lx, ly, x0 + inset, y0 - grab / 2, ew, grab)) {
        if (out_handle) {
            *out_handle = UI_ENTITY_HB_N;
        }
        return 1;
    }
    if (ew > 0 && point_in_rect(lx, ly, x0 + inset, y1 - grab / 2, ew, grab)) {
        if (out_handle) {
            *out_handle = UI_ENTITY_HB_S;
        }
        return 1;
    }
    if (eh > 0 && point_in_rect(lx, ly, x1 - grab / 2, y0 + inset, grab, eh)) {
        if (out_handle) {
            *out_handle = UI_ENTITY_HB_E;
        }
        return 1;
    }
    if (eh > 0 && point_in_rect(lx, ly, x0 - grab / 2, y0 + inset, grab, eh)) {
        if (out_handle) {
            *out_handle = UI_ENTITY_HB_W;
        }
        return 1;
    }
    return 0;
}

static int entity_hb_handle_cursor(int handle) {
    if (handle == UI_ENTITY_HB_NW) {
        return UI_ENTITY_HB_CUR_NWSE;
    }
    if (handle == UI_ENTITY_HB_NE) {
        return UI_ENTITY_HB_CUR_NESW;
    }
    if (handle == UI_ENTITY_HB_SE) {
        return UI_ENTITY_HB_CUR_SE;
    }
    if (handle == UI_ENTITY_HB_SW) {
        return UI_ENTITY_HB_CUR_SW;
    }
    if (handle == UI_ENTITY_HB_E || handle == UI_ENTITY_HB_W) {
        return UI_ENTITY_HB_CUR_WE;
    }
    if (handle == UI_ENTITY_HB_N || handle == UI_ENTITY_HB_S) {
        return UI_ENTITY_HB_CUR_NS;
    }
    return UI_ENTITY_HB_CUR_NONE;
}

int entity_edit_guides_cursor(const UiState *ui, int lx, int ly) {
    EntityModalLayout lo;
    const R01EntityState *st;
    const R01EntityFrame *fr = NULL;
    int handle = 0;
    if (!ui || !ui->entity_edit.open || ui->entity_edit.preview_playing ||
        ui->entity_edit.tool != UI_ENTITY_TOOL_GUIDES) {
        return UI_ENTITY_HB_CUR_NONE;
    }
    if (ui->entity_edit.dragging == 4) {
        return entity_hb_handle_cursor(ui->entity_edit.drag_corner);
    }
    if (ui->entity_edit.dragging == 3) {
        return UI_ENTITY_HB_CUR_MOVE;
    }
    entity_modal_layout(ui, &lo);
    if (ui->entity_edit.state < 0 || ui->entity_edit.state >= ui->entity_edit.draft.state_count ||
        ui->entity_edit.state >= R01_ENTITY_STATES_MAX) {
        return UI_ENTITY_HB_CUR_NONE;
    }
    st = &ui->entity_edit.draft.states[ui->entity_edit.state];
    if (ui->entity_edit.frame >= 0 && ui->entity_edit.frame < st->frame_count &&
        ui->entity_edit.frame < R01_ENTITY_FRAMES_MAX) {
        fr = &st->frames[ui->entity_edit.frame];
    }
    if (fr && entity_edit_origin_hit(ui, &lo, fr, lx, ly)) {
        return UI_ENTITY_HB_CUR_NONE;
    }
    if (entity_edit_hitbox_handle_hit(ui, &lo, st, lx, ly, &handle)) {
        return entity_hb_handle_cursor(handle);
    }
    if (entity_edit_hitbox_body_hit(ui, &lo, st, lx, ly)) {
        return UI_ENTITY_HB_CUR_MOVE;
    }
    return UI_ENTITY_HB_CUR_NONE;
}

int entity_edit_hitbox_body_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityState *st, int lx,
                                  int ly) {
    int x0, y0, x1, y1;
    if (!st || st->hitbox_w < 1 || st->hitbox_h < 1) {
        return 0;
    }
    entity_edit_world_to_screen(ui, lo, st->hitbox_x, st->hitbox_y, &x0, &y0);
    entity_edit_world_to_screen(ui, lo, st->hitbox_x + st->hitbox_w, st->hitbox_y + st->hitbox_h, &x1, &y1);
    return point_in_rect(lx, ly, x0, y0, x1 - x0, y1 - y0);
}

static void entity_view_max(const UiState *ui, int *out_max_x, int *out_max_y) {
    int sc = entity_edit_compose_scale(ui);
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

void entity_edit_view_clamp(UiState *ui) {
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

void entity_edit_view_pan(UiState *ui, int dx, int dy) {
    if (!ui) {
        return;
    }
    ui->entity_edit.view_x += dx;
    ui->entity_edit.view_y += dy;
    entity_edit_view_clamp(ui);
}

void entity_edit_set_zoom(UiState *ui, const EntityModalLayout *lo, int new_zoom, int focus_lx, int focus_ly) {
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
    entity_edit_screen_to_world(ui, lo, focus_lx, focus_ly, &wx, &wy);
    ui->entity_edit.zoom = new_zoom;
    new_sc = entity_edit_compose_scale(ui);
    /* Keep the world pixel under the cursor stable across zoom. */
    ui->entity_edit.view_x = wx * new_sc - (focus_lx - lo->right_grid_x);
    ui->entity_edit.view_y = wy * new_sc - (focus_ly - lo->right_grid_y);
    entity_edit_view_clamp(ui);
}

void entity_edit_recompute_guides(UiState *ui) {
    R01EntityFrame *fr = entity_edit_frame(ui);
    R01EntityState *st = entity_edit_state(ui);
    if (fr) {
        r01_entity_frame_recompute_guides(fr);
    }
    if (st) {
        r01_entity_state_clamp_hitbox(st);
    }
}

static void entity_sync_catalog_pal(R01Project *p, const R01EntityPart *pt) {
    int i;
    if (!p || !pt) {
        return;
    }
    for (i = 0; i < p->sprite_count; i++) {
        if (p->sprites[i].bank == pt->bank && p->sprites[i].tile_id == pt->tile_id) {
            (void)r01_world_sprite_set_pal(p, i, pt->pal & 3);
            return;
        }
    }
}

void entity_edit_apply_pal_to_part(UiState *ui, R01EntityPart *pt, int pal) {
    if (!ui || !pt) {
        return;
    }
    pt->pal = pal & 3;
    entity_sync_catalog_pal(ui->project, pt);
}

void entity_edit_clear_sel(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.sel_mask = 0;
}

static void entity_edit_sel_refresh_primary(UiState *ui, const R01EntityFrame *fr) {
    int i;
    ui->entity_edit.sel_part = -1;
    if (!fr) {
        return;
    }
    for (i = fr->part_count - 1; i >= 0; i--) {
        if (ui->entity_edit.sel_mask & (1u << i)) {
            ui->entity_edit.sel_part = i;
            ui->entity_edit.paint_pal = fr->parts[i].pal & 3;
            break;
        }
    }
}

void entity_edit_select_part(UiState *ui, R01EntityFrame *fr, int idx) {
    if (!ui) {
        return;
    }
    if (!fr || idx < 0 || idx >= fr->part_count) {
        entity_edit_clear_sel(ui);
        return;
    }
    ui->entity_edit.sel_part = idx;
    ui->entity_edit.sel_mask = 1u << idx;
    ui->entity_edit.paint_pal = fr->parts[idx].pal & 3;
}

void entity_edit_toggle_part(UiState *ui, R01EntityFrame *fr, int idx) {
    unsigned bit;
    if (!ui || !fr || idx < 0 || idx >= fr->part_count) {
        return;
    }
    bit = 1u << idx;
    if (ui->entity_edit.sel_mask & bit) {
        ui->entity_edit.sel_mask &= ~bit;
        if (ui->entity_edit.sel_part == idx) {
            entity_edit_sel_refresh_primary(ui, fr);
        }
    } else {
        ui->entity_edit.sel_mask |= bit;
        ui->entity_edit.sel_part = idx;
        ui->entity_edit.paint_pal = fr->parts[idx].pal & 3;
    }
}

void entity_edit_select_all_parts(UiState *ui) {
    R01EntityFrame *fr;
    int i;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    fr = entity_edit_frame(ui);
    ui->entity_edit.sel_mask = 0;
    ui->entity_edit.sel_part = -1;
    if (!fr) {
        return;
    }
    for (i = 0; i < fr->part_count; i++) {
        ui->entity_edit.sel_mask |= 1u << i;
    }
    entity_edit_sel_refresh_primary(ui, fr);
}

void entity_edit_select_rect(UiState *ui, R01EntityFrame *fr, int x0, int y0, int x1, int y1, int add) {
    int i;
    unsigned mask;
    if (!ui) {
        return;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    mask = add ? ui->entity_edit.sel_mask : 0u;
    if (fr) {
        for (i = 0; i < fr->part_count; i++) {
            int xa = fr->parts[i].dx;
            int ya = fr->parts[i].dy;
            int xb = xa + 8;
            int yb = ya + 8;
            if (xa < x1 && xb > x0 && ya < y1 && yb > y0) {
                mask |= 1u << i;
            }
        }
    }
    ui->entity_edit.sel_mask = mask;
    entity_edit_sel_refresh_primary(ui, fr);
}

void entity_edit_remove_selected(UiState *ui) {
    R01EntityFrame *fr;
    int i;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    fr = entity_edit_frame(ui);
    if (!fr || ui->entity_edit.sel_mask == 0) {
        return;
    }
    for (i = fr->part_count - 1; i >= 0; i--) {
        R01EntityPart removed;
        if ((ui->entity_edit.sel_mask & (1u << i)) == 0) {
            continue;
        }
        removed = fr->parts[i];
        r01_entity_frame_remove_part(fr, i);
        ui_undo_push_entity_part_remove(ui, ui->entity_edit.state, ui->entity_edit.frame, i, &removed);
    }
    entity_edit_clear_sel(ui);
    entity_edit_recompute_guides(ui);
}

void entity_edit_copy_parts(UiState *ui) {
    R01EntityFrame *fr;
    int i;
    int n;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    fr = entity_edit_frame(ui);
    n = 0;
    if (fr) {
        for (i = 0; i < fr->part_count && n < R01_ENTITY_PARTS_MAX; i++) {
            if (ui->entity_edit.sel_mask & (1u << i)) {
                ui->entity_edit.clip[n++] = fr->parts[i];
            }
        }
    }
    if (n < 1) {
        ui_toast(ui, "select a sprite first", 1);
        return;
    }
    ui->entity_edit.clip_count = n;
    ui_toast(ui, "sprites copied", 0);
}

static int entity_edit_paste_parts(UiState *ui) {
    R01EntityFrame *fr;
    unsigned mask;
    int i;
    int n;
    int added;
    if (!ui || ui->entity_edit.clip_count < 1) {
        return -1;
    }
    fr = entity_edit_frame(ui);
    if (!fr) {
        return -1;
    }
    n = ui->entity_edit.clip_count;
    if (n > R01_ENTITY_PARTS_MAX - fr->part_count) {
        n = R01_ENTITY_PARTS_MAX - fr->part_count;
    }
    if (n < 1) {
        return 0;
    }
    mask = 0;
    added = 0;
    for (i = 0; i < n; i++) {
        int idx = r01_entity_frame_add_part(fr, &ui->entity_edit.clip[i]);
        if (idx < 0) {
            break;
        }
        mask |= 1u << idx;
        added++;
        ui_undo_push_entity_part_add(ui, ui->entity_edit.state, ui->entity_edit.frame, idx,
                                     &ui->entity_edit.clip[i], -1);
    }
    if (added > 0) {
        ui->entity_edit.sel_mask = mask;
        entity_edit_sel_refresh_primary(ui, fr);
        entity_edit_recompute_guides(ui);
    }
    return 0;
}

void entity_edit_paint_at(UiState *ui, R01World *w, R01EntityFrame *fr, int idx, int cx, int cy) {
    R01EntityPart *pt;
    if (!ui || !w || !fr || idx < 0 || idx >= fr->part_count) {
        return;
    }
    pt = &fr->parts[idx];
    entity_edit_apply_pal_to_part(ui, pt, ui->entity_edit.paint_pal);
    entity_edit_select_part(ui, fr, idx);
    ui_undo_spr_paint_touch_tile(ui, pt->bank, pt->tile_id);
    (void)ui_compose_paint_brush(ui->project, w, pt, cx, cy, ui->entity_edit.paint_color,
                                 ui->entity_edit.brush_size);
}

int entity_edit_paste_clipboard(UiState *ui) {
    R01World *w;
    R01EntityFrame *fr;
    R01EntityPart *pt;
    const uint8_t *src;
    uint8_t chr[R01_TILE_BYTES];
    int pal;

    if (!ui || !ui->entity_edit.open || !ui->project) {
        return -1;
    }
    if (ui->entity_edit.clip_count > 0) {
        return entity_edit_paste_parts(ui);
    }
    w = r01_project_active_world(ui->project);
    fr = entity_edit_frame(ui);
    if (!w || !fr || ui->entity_edit.sel_part < 0 || ui->entity_edit.sel_part >= fr->part_count) {
        ui_toast(ui, "select a sprite first", 1);
        return -1;
    }
    pt = &fr->parts[ui->entity_edit.sel_part];
    pal = pt->pal & 3;
    src = r01_chr_resolve_spr(ui->project, w, pt->bank, pt->tile_id);
    if (src) {
        memcpy(chr, src, R01_TILE_BYTES);
    } else {
        memset(chr, 0, R01_TILE_BYTES);
    }
    (void)ui_undo_spr_paint_begin(ui);
    ui_undo_spr_paint_touch_tile(ui, pt->bank, pt->tile_id);
    if (ui_paste_clipboard_png_tile(ui, chr, pal, 1) != 0) {
        ui_undo_spr_paint_end(ui);
        return -1;
    }
    if (r01_chr_write_resolved_spr(ui->project, w, pt->bank, pt->tile_id, chr) != 0) {
        ui_undo_spr_paint_end(ui);
        ui_toast(ui, "cannot write sprite CHR", 1);
        return -1;
    }
    ui->entity_edit.paint_pal = pal;
    ui_undo_spr_paint_end(ui);
    return 0;
}
