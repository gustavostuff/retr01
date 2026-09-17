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
int entity_edit_hitbox_corner_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx,
                                    int ly, int *out_corner) {
    int sc = entity_edit_compose_scale(ui);
    int grab = sc > UI_UNIT ? sc : UI_UNIT;
    int i;
    int cx[4], cy[4];
    if (!fr || fr->hitbox_w < 1 || fr->hitbox_h < 1) {
        return 0;
    }
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x, fr->hitbox_y, &cx[0], &cy[0]);
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x + fr->hitbox_w, fr->hitbox_y, &cx[1], &cy[1]);
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x + fr->hitbox_w, fr->hitbox_y + fr->hitbox_h, &cx[2], &cy[2]);
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x, fr->hitbox_y + fr->hitbox_h, &cx[3], &cy[3]);
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

int entity_edit_hitbox_body_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx,
                                  int ly) {
    int x0, y0, x1, y1;
    if (!fr || fr->hitbox_w < 1 || fr->hitbox_h < 1) {
        return 0;
    }
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x, fr->hitbox_y, &x0, &y0);
    entity_edit_world_to_screen(ui, lo, fr->hitbox_x + fr->hitbox_w, fr->hitbox_y + fr->hitbox_h, &x1, &y1);
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
    if (fr) {
        r01_entity_frame_recompute_guides(fr);
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

void entity_edit_apply_pal_to_part(UiState *ui, R01EntityPart *pt, int pal) {
    R01World *w;
    if (!ui || !pt) {
        return;
    }
    pt->pal = pal & 3;
    w = r01_project_active_world(ui->project);
    entity_sync_catalog_pal(w, pt);
}

void entity_edit_select_part(UiState *ui, R01EntityFrame *fr, int idx) {
    if (!ui || !fr || idx < 0 || idx >= fr->part_count) {
        ui->entity_edit.sel_part = -1;
        return;
    }
    ui->entity_edit.sel_part = idx;
    ui->entity_edit.paint_pal = fr->parts[idx].pal & 3;
}

void entity_edit_paint_at(UiState *ui, R01World *w, R01EntityFrame *fr, int idx, int cx, int cy) {
    R01EntityPart *pt;
    if (!ui || !w || !fr || idx < 0 || idx >= fr->part_count) {
        return;
    }
    pt = &fr->parts[idx];
    entity_edit_apply_pal_to_part(ui, pt, ui->entity_edit.paint_pal);
    ui->entity_edit.sel_part = idx;
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
