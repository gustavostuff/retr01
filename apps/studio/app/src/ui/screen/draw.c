#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"
#include "retr01_emu/types.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_spr_tile_px(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityPart *pt, int log_x,
                             int log_y, int ox, int oy, int scale, int clip_viewport) {
    const uint8_t *raw;
    uint8_t oriented[R01_TILE_BYTES];
    int row = w->default_pal_row;
    int sy, sx;
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    raw = r01_chr_resolve_spr(ui->project, w, pt->bank, pt->tile_id);
    if (!raw) {
        return;
    }
    r01_tile_orient(raw, pt->flip_h, pt->flip_v, oriented);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(oriented, sx, sy);
            uint8_t cr, cg, cb;
            int vx = log_x + sx;
            int vy = log_y + sy;
            SDL_Rect px;
            if (col == 0) {
                continue;
            }
            if (clip_viewport && (vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H)) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            px.x = ox + vx * scale;
            px.y = oy + vy * scale;
            px.w = scale;
            px.h = scale;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
}

static int entity_local_bounds(const R01EntityType *ent, int local_x, int local_y, int flip_h, int flip_v,
                               int *out_min_x, int *out_min_y, int *out_max_x, int *out_max_y) {
    const R01EntityState *st;
    const R01EntityFrame *fr;
    int pi;
    int min_x = 9999, min_y = 9999, max_x = -9999, max_y = -9999;
    if (!ent || ent->state_count < 1 || ent->states[0].frame_count < 1) {
        return 0;
    }
    st = &ent->states[0];
    fr = &st->frames[0];
    for (pi = 0; pi < fr->part_count; pi++) {
        const R01EntityPart *pt = &fr->parts[pi];
        int dx, dy;
        int px, py;
        r01_entity_part_instance_pose(fr, pt, flip_h, flip_v, &dx, &dy, NULL, NULL);
        px = r01_entity_world_x(local_x, fr->origin_x, dx);
        py = r01_entity_world_y(local_y, fr->origin_y, dy);
        if (px < min_x) {
            min_x = px;
        }
        if (py < min_y) {
            min_y = py;
        }
        if (px + 8 > max_x) {
            max_x = px + 8;
        }
        if (py + 8 > max_y) {
            max_y = py + 8;
        }
    }
    if (max_x <= min_x) {
        return 0;
    }
    if (out_min_x) {
        *out_min_x = min_x;
    }
    if (out_min_y) {
        *out_min_y = min_y;
    }
    if (out_max_x) {
        *out_max_x = max_x;
    }
    if (out_max_y) {
        *out_max_y = max_y;
    }
    return 1;
}

static void draw_screen_outline_px(UiState *ui, SDL_Renderer *r, int ox, int oy, int min_x, int min_y, int max_x,
                                   int max_y, Uint8 alpha) {
    int sel_x = min_x;
    int sel_y = min_y;
    int sel_w = max_x - min_x;
    int sel_h = max_y - min_y;
    int sc = ui_screen_scale(ui);
    if (sel_x < 0) {
        sel_w += sel_x;
        sel_x = 0;
    }
    if (sel_y < 0) {
        sel_h += sel_y;
        sel_y = 0;
    }
    if (sel_x + sel_w > R01_SCREEN_PX_W) {
        sel_w = R01_SCREEN_PX_W - sel_x;
    }
    if (sel_y + sel_h > R01_SCREEN_PX_H) {
        sel_h = R01_SCREEN_PX_H - sel_y;
    }
    if (sel_w > 0 && sel_h > 0) {
        draw_marching_ants_a(r, ox + sel_x * sc, oy + sel_y * sc, sel_w * sc, sel_h * sc, alpha);
    }
}

static void draw_entity_at_screen(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityType *ent,
                                  int local_x, int local_y, int flip_h, int flip_v, int ox, int oy, int selected) {
    const R01EntityState *st;
    const R01EntityFrame *fr;
    int pi;
    int min_x = 9999, min_y = 9999, max_x = -9999, max_y = -9999;
    if (!ent || ent->state_count < 1 || ent->states[0].frame_count < 1) {
        return;
    }
    st = &ent->states[0];
    fr = &st->frames[0];
    for (pi = 0; pi < fr->part_count; pi++) {
        const R01EntityPart *pt = &fr->parts[pi];
        R01EntityPart draw_pt;
        int dx, dy, fh, fv;
        int px, py;
        r01_entity_part_instance_pose(fr, pt, flip_h, flip_v, &dx, &dy, &fh, &fv);
        px = r01_entity_world_x(local_x, fr->origin_x, dx);
        py = r01_entity_world_y(local_y, fr->origin_y, dy);
        draw_pt = *pt;
        draw_pt.flip_h = fh;
        draw_pt.flip_v = fv;
        draw_spr_tile_px(ui, r, w, &draw_pt, px, py, ox, oy, ui_screen_scale(ui), 1);
        if (px < min_x) {
            min_x = px;
        }
        if (py < min_y) {
            min_y = py;
        }
        if (px + 8 > max_x) {
            max_x = px + 8;
        }
        if (py + 8 > max_y) {
            max_y = py + 8;
        }
    }
    if (selected && max_x > min_x) {
        draw_screen_outline_px(ui, r, ox, oy, min_x, min_y, max_x, max_y, 255);
    }
}

static void draw_instances_on_screen(UiState *ui, SDL_Renderer *r, const R01World *w, const R01Screen *s, int ox,
                                     int oy) {
    const R01Project *p = ui ? ui->project : NULL;
    int i;
    if (!w || !s || !p) {
        return;
    }
    for (i = 0; i < w->instance_count; i++) {
        const R01EntityInstance *inst = &w->instances[i];
        const R01EntityType *ent;
        int local_x = inst->world_x - s->col * R01_SCREEN_PX_W;
        int local_y = inst->world_y - s->row * R01_SCREEN_PX_H;
        int min_x, min_y, max_x, max_y;
        if (inst->type_id < 0 || inst->type_id >= p->entity_count) {
            continue;
        }
        ent = &p->entities[inst->type_id];
        if (!entity_local_bounds(ent, local_x, local_y, inst->flip_h, inst->flip_v, &min_x, &min_y, &max_x,
                                 &max_y)) {
            continue;
        }
        if (max_x <= 0 || max_y <= 0 || min_x >= R01_SCREEN_PX_W || min_y >= R01_SCREEN_PX_H) {
            continue;
        }
        draw_entity_at_screen(ui, r, w, ent, local_x, local_y, inst->flip_h, inst->flip_v, ox, oy,
                              i == ui->sel_instance);
    }
}

int instance_hit_on_screen(const UiState *ui, int lx, int ly, int *out_inst) {
    R01World *w;
    R01Screen *s;
    int px, py;
    int i;
    if (!ui || ui->play.active || ui->hide_spr_layer) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    s = r01_project_active_screen(ui->project);
    if (!w || !s || !screen_pixel_hit(ui, lx, ly, &px, &py)) {
        return 0;
    }
    for (i = w->instance_count - 1; i >= 0; i--) {
        const R01EntityInstance *inst = &w->instances[i];
        const R01EntityType *ent;
        const R01EntityState *st;
        const R01EntityFrame *fr;
        int local_x, local_y, pi;
        if (inst->type_id < 0 || inst->type_id >= p->entity_count) {
            continue;
        }
        ent = &p->entities[inst->type_id];
        if (ent->state_count < 1 || ent->states[0].frame_count < 1) {
            continue;
        }
        st = &ent->states[0];
        fr = &st->frames[0];
        local_x = inst->world_x - s->col * R01_SCREEN_PX_W;
        local_y = inst->world_y - s->row * R01_SCREEN_PX_H;
        for (pi = 0; pi < fr->part_count; pi++) {
            const R01EntityPart *pt = &fr->parts[pi];
            int dx, dy;
            int part_x, part_y;
            r01_entity_part_instance_pose(fr, pt, inst->flip_h, inst->flip_v, &dx, &dy, NULL, NULL);
            part_x = r01_entity_world_x(local_x, fr->origin_x, dx);
            part_y = r01_entity_world_y(local_y, fr->origin_y, dy);
            if (px >= part_x && px < part_x + 8 && py >= part_y && py < part_y + 8) {
                if (out_inst) {
                    *out_inst = i;
                }
                return 1;
            }
        }
    }
    return 0;
}

static void set_viewport_clip(SDL_Renderer *r, const UiState *ui, int ox, int oy) {
    SDL_Rect clip = {ox, oy, ui_screen_w(ui), ui_screen_h(ui)};
    SDL_RenderSetClipRect(r, &clip);
}

static void draw_warp_markers(UiState *ui, SDL_Renderer *r, const R01World *w, const R01Screen *s, int ox,
                              int oy) {
    int i;
    if (!w || !s) {
        return;
    }
    for (i = 0; i < w->warp_entrance_count; i++) {
        const R01WarpEntrance *we = &w->warp_entrances[i];
        SDL_Rect tile;
        if (!we->present || we->screen_col != s->col || we->screen_row != s->row) {
            continue;
        }
        tile.x = ox + we->tile_col * 8 * ui_screen_scale(ui);
        tile.y = oy + we->tile_row * 8 * ui_screen_scale(ui);
        tile.w = 8 * ui_screen_scale(ui);
        tile.h = 8 * ui_screen_scale(ui);
        SDL_SetRenderDrawColor(r, 80, 220, 120, 255);
        SDL_RenderDrawRect(r, &tile);
        SDL_RenderDrawRect(r, &tile);
    }
    for (i = 0; i < w->warp_exit_count; i++) {
        const R01WarpExit *wx = &w->warp_exits[i];
        SDL_Rect tile;
        if (!wx->present || wx->dest_screen_col != s->col || wx->dest_screen_row != s->row) {
            continue;
        }
        tile.x = ox + wx->dest_tile_col * 8 * ui_screen_scale(ui);
        tile.y = oy + wx->dest_tile_row * 8 * ui_screen_scale(ui);
        tile.w = 8 * ui_screen_scale(ui);
        tile.h = 8 * ui_screen_scale(ui);
        SDL_SetRenderDrawColor(r, 120, 160, 255, 255);
        SDL_RenderDrawRect(r, &tile);
        SDL_RenderDrawRect(r, &tile);
    }
    (void)ui;
}

static void draw_bg_tile_ghost(UiState *ui, SDL_Renderer *r, const R01World *w, uint8_t tile_id, uint8_t attr,
                               int tile_x, int tile_y, int ox, int oy) {
    const R01Project *p = ui ? ui->project : NULL;
    const uint8_t *raw;
    uint8_t oriented[R01_TILE_BYTES];
    int bank = r01_attr_bank(attr);
    int pal = r01_attr_pal(attr);
    int row = w ? w->default_pal_row : 0;
    int sc = ui_screen_scale(ui);
    int sy, sx;
    if (!w || !p || tile_x < 0 || tile_y < 0 || tile_x >= R01_SCREEN_TILES_X || tile_y >= R01_SCREEN_TILES_Y) {
        return;
    }
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    if (bank < 0 || bank >= R01_BG_BANKS || tile_id >= (uint8_t)p->bg_banks[bank].tile_count) {
        return;
    }
    raw = p->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
    r01_tile_orient(raw, r01_attr_flip_h(attr), r01_attr_flip_v(attr), oriented);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(oriented, sx, sy);
            uint8_t cr, cg, cb;
            SDL_Rect px;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_bg[row][pal & 3].idx[col & 3u], &cr, &cg, &cb);
            px.x = ox + (tile_x * 8 + sx) * sc;
            px.y = oy + (tile_y * 8 + sy) * sc;
            px.w = sc;
            px.h = sc;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 128);
            SDL_RenderFillRect(r, &px);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_paint_ghost(UiState *ui, SDL_Renderer *r, const R01World *w, int ox, int oy) {
    int tx, ty;
    int y, x;
    int hover_inst;
    if (!ui || !w || ui->play.active || !ui_work_allows_bg(ui) || ui->hide_bg_layer) {
        return;
    }
    if (!(SDL_GetModState() & KMOD_CTRL) || (SDL_GetModState() & KMOD_ALT) || ui->keys[SDL_SCANCODE_F]) {
        return;
    }
    if (!ui->paint_stamp_valid || ui->paint_stamp_w < 1 || ui->paint_stamp_h < 1) {
        return;
    }
    if (!screen_hit(ui, ui->mouse_x, ui->mouse_y, &tx, &ty)) {
        return;
    }
    if (ui_work_allows_spr(ui) && !ui->hide_spr_layer && instance_hit_on_screen(ui, ui->mouse_x, ui->mouse_y, &hover_inst)) {
        return;
    }
    for (y = 0; y < ui->paint_stamp_h; y++) {
        for (x = 0; x < ui->paint_stamp_w; x++) {
            int si = y * ui->paint_stamp_w + x;
            draw_bg_tile_ghost(ui, r, w, ui->paint_stamp_tiles[si], ui->paint_stamp_attrs[si], tx + x, ty + y, ox,
                               oy);
        }
    }
}

static void draw_hover_and_sel_overlays(UiState *ui, SDL_Renderer *r, const R01World *w, const R01Screen *s, int ox,
                                        int oy, int plane_bg0) {
    const R01Project *p = ui ? ui->project : NULL;
    int tx, ty;
    int hover_inst = -1;
    int over_screen;
    int sc = ui_screen_scale(ui);

    if (!ui || ui->play.active) {
        return;
    }

    /* Selection: tile rect at full opacity. */
    if (screen_sel_valid(ui) && ui->sel_instance < 0) {
        int min_x, min_y, max_x, max_y;
        int sx, sy, sw, sh;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        sx = ox + min_x * 8 * sc;
        sy = oy + min_y * 8 * sc;
        sw = (max_x - min_x + 1) * 8 * sc;
        sh = (max_y - min_y + 1) * 8 * sc;
        draw_marching_ants_a(r, sx, sy, sw, sh, 255);
    }

    over_screen = screen_hit(ui, ui->mouse_x, ui->mouse_y, &tx, &ty);
    if (!over_screen) {
        return;
    }

    if (!plane_bg0 && ui_work_allows_spr(ui) && !ui->hide_spr_layer &&
        instance_hit_on_screen(ui, ui->mouse_x, ui->mouse_y, &hover_inst)) {
        if (hover_inst != ui->sel_instance && w && s && hover_inst >= 0 && hover_inst < w->instance_count) {
            const R01EntityInstance *inst = &w->instances[hover_inst];
            const R01EntityType *ent;
            int local_x, local_y, min_x, min_y, max_x, max_y;
            if (inst->type_id >= 0 && inst->type_id < p->entity_count) {
                ent = &p->entities[inst->type_id];
                local_x = inst->world_x - s->col * R01_SCREEN_PX_W;
                local_y = inst->world_y - s->row * R01_SCREEN_PX_H;
                if (entity_local_bounds(ent, local_x, local_y, inst->flip_h, inst->flip_v, &min_x, &min_y, &max_x,
                                        &max_y)) {
                    draw_screen_outline_px(ui, r, ox, oy, min_x, min_y, max_x, max_y, 128);
                }
            }
        }
        return; /* entity hover wins over tile hover in Both mode */
    }

    if (ui_work_allows_bg(ui) && !ui->hide_bg_layer && ui->sel_instance < 0) {
        int paint_ghost =
            (SDL_GetModState() & KMOD_CTRL) && !(SDL_GetModState() & KMOD_ALT) && !ui->keys[SDL_SCANCODE_F] &&
            ui->paint_stamp_valid && ui->paint_stamp_w > 0 && ui->paint_stamp_h > 0;
        int in_sel = 0;
        if (paint_ghost) {
            return;
        }
        if (screen_sel_valid(ui)) {
            int min_x, min_y, max_x, max_y;
            screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
            in_sel = tx >= min_x && tx <= max_x && ty >= min_y && ty <= max_y;
        }
        if (!in_sel) {
            draw_marching_ants_a(r, ox + tx * 8 * sc, oy + ty * 8 * sc, 8 * sc, 8 * sc, 128);
        }
    }
}

static void format_tile_inspect(char *buf, size_t cap, const R01Screen *s, int tx, int ty, int plane_bg0) {
    int cell;
    uint8_t tile;
    uint8_t attr;
    if (!buf || cap < 1 || !s || tx < 0 || ty < 0 || tx >= R01_SCREEN_TILES_X || ty >= R01_SCREEN_TILES_Y) {
        return;
    }
    cell = ty * R01_SCREEN_TILES_X + tx;
    tile = s->tiles[cell];
    attr = s->attrs[cell];
    snprintf(buf, cap, "%s tile (%d,%d) id=%u bank=%d pal=%d attr=$%02X%s%s%s", plane_bg0 ? "BG0" : "BG1", tx, ty,
             (unsigned)tile, r01_attr_bank(attr), r01_attr_pal(attr), (unsigned)attr,
             s->solids[cell] ? " solid" : "", r01_attr_flip_h(attr) ? " H" : "",
             r01_attr_flip_v(attr) ? " V" : "");
}

static void format_tile_multisel(char *buf, size_t cap, int plane_bg0, int x0, int y0, int x1, int y1) {
    if (!buf || cap < 1) {
        return;
    }
    snprintf(buf, cap, "%s (%d,%d) to (%d,%d) multiselection", plane_bg0 ? "BG0" : "BG1", x0, y0, x1, y1);
}

static void format_entity_inspect(char *buf, size_t cap, const R01Project *p, const R01World *w, int inst_idx) {
    const R01EntityInstance *inst;
    const R01EntityType *ent;
    const char *name;
    if (!buf || cap < 1 || !w || inst_idx < 0 || inst_idx >= w->instance_count) {
        return;
    }
    inst = &w->instances[inst_idx];
    if (inst->type_id >= 0 && inst->type_id < p->entity_count) {
        ent = &p->entities[inst->type_id];
        name = ent->name[0] ? ent->name : "?";
    } else {
        name = "?";
    }
    snprintf(buf, cap, "entity #%d \"%s\" type=%d (%d,%d)%s%s", inst_idx, name, inst->type_id, inst->world_x,
             inst->world_y, inst->flip_h ? " H" : "", inst->flip_v ? " V" : "");
}

static void preview_inspect_layout(const UiState *ui, int *out_x, int *out_y, int *out_text_w, int *out_btn_x,
                                   int *out_btn_w) {
    int ox, oy, y, sw, btn_w;
    screen_origin(ui, &ox, &oy);
    sw = ui_screen_w(ui);
    btn_w = label_width("Copy");
    if (btn_w < UI_UNIT * 4) {
        btn_w = UI_UNIT * 4;
    }
    y = oy + ui_screen_h(ui) + UI_UNIT;
    if (y + UI_BTN_H * 2 > ui_logic_h(ui) - UI_UNIT) {
        y = ui_logic_h(ui) - UI_BTN_H * 2 - UI_UNIT;
    }
    if (out_x) {
        *out_x = ox;
    }
    if (out_y) {
        *out_y = y;
    }
    if (out_text_w) {
        *out_text_w = sw;
    }
    if (out_btn_x) {
        *out_btn_x = ox + sw + UI_UNIT;
    }
    if (out_btn_w) {
        *out_btn_w = btn_w;
    }
}

/* Split status into at most 2 lines that fit max_w (font pixels). Prefer a space break. */
static void preview_inspect_split2(const char *text, int max_w, char *line1, size_t l1cap, char *line2,
                                   size_t l2cap) {
    int n;
    int i;
    int fit = 0;
    int break_at = -1;
    if (!text || !line1 || !line2 || l1cap < 2 || l2cap < 1) {
        return;
    }
    line1[0] = '\0';
    line2[0] = '\0';
    n = (int)strlen(text);
    if (n < 1) {
        return;
    }
    if (font_text_width(text) <= max_w) {
        snprintf(line1, l1cap, "%s", text);
        return;
    }
    for (i = 1; i <= n; i++) {
        if (font_text_width_n(text, i) > max_w) {
            break;
        }
        fit = i;
        if (i < n && text[i] == ' ') {
            break_at = i;
        }
    }
    if (fit < 1) {
        fit = 1;
    }
    if (break_at > 0) {
        fit = break_at;
    }
    if ((size_t)fit >= l1cap) {
        fit = (int)l1cap - 1;
    }
    memcpy(line1, text, (size_t)fit);
    line1[fit] = '\0';
    while (fit < n && text[fit] == ' ') {
        fit++;
    }
    snprintf(line2, l2cap, "%s", text + fit);
}

int preview_inspect_copy_hit(const UiState *ui, int lx, int ly) {
    int x, y, tw, bx, bw;
    if (!ui || ui->play.active || ui->app_mode != UI_APP_GRAPHICS || !ui->preview_inspect[0]) {
        return 0;
    }
    preview_inspect_layout(ui, &x, &y, &tw, &bx, &bw);
    (void)x;
    (void)tw;
    return point_in_rect(lx, ly, bx, y, bw, UI_BTN_H);
}

void preview_inspect_copy(UiState *ui) {
    if (!ui || !ui->preview_inspect[0]) {
        return;
    }
    if (SDL_SetClipboardText(ui->preview_inspect) != 0) {
        ui_toast(ui, "copy failed", 1);
        return;
    }
    ui_toast(ui, "copied", 0);
}

void ui_preview_inspect_refresh(UiState *ui) {
    R01Project *p;
    R01World *w;
    R01Screen *s;
    char live[sizeof(ui->preview_inspect)];
    int tx, ty;
    int inst;
    int plane_bg0;

    if (!ui || !ui->project || ui->play.active || ui->app_mode != UI_APP_GRAPHICS) {
        return;
    }
    p = ui->project;
    w = r01_project_active_world(ui->project);
    s = ui_edit_map_screen(ui);
    plane_bg0 = (ui->worlds_plane == UI_WORLDS_PLANE_BG0);
    live[0] = '\0';

    if (s && w && screen_hit(ui, ui->mouse_x, ui->mouse_y, &tx, &ty)) {
        if (!plane_bg0 && ui_work_allows_spr(ui) && !ui->hide_spr_layer &&
            instance_hit_on_screen(ui, ui->mouse_x, ui->mouse_y, &inst)) {
            format_entity_inspect(live, sizeof(live), p, w, inst);
        } else if (ui_work_allows_bg(ui) && !ui->hide_bg_layer) {
            format_tile_inspect(live, sizeof(live), s, tx, ty, plane_bg0);
        }
    }

    if (live[0]) {
        memcpy(ui->preview_inspect, live, sizeof(ui->preview_inspect));
        return;
    }

    /* Not hovering: keep showing the last click/selection. */
    live[0] = '\0';
    if (!plane_bg0 && ui->sel_instance >= 0 && w) {
        format_entity_inspect(live, sizeof(live), p, w, ui->sel_instance);
    } else if (s && screen_sel_valid(ui) && ui->sel_instance < 0) {
        int min_x, min_y, max_x, max_y;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        if (min_x != max_x || min_y != max_y) {
            format_tile_multisel(live, sizeof(live), plane_bg0, min_x, min_y, max_x, max_y);
        } else {
            format_tile_inspect(live, sizeof(live), s, min_x, min_y, plane_bg0);
        }
    }
    if (live[0]) {
        memcpy(ui->preview_inspect, live, sizeof(ui->preview_inspect));
    }
}

void draw_preview_inspect(UiState *ui, SDL_Renderer *r) {
    int x, y, text_w, btn_x, btn_w;
    char line1[160];
    char line2[160];
    int hover;
    int lh;
    if (!ui || !r || ui->play.active || ui->app_mode != UI_APP_GRAPHICS) {
        return;
    }
    ui_preview_inspect_refresh(ui);
    if (!ui->preview_inspect[0]) {
        return;
    }
    preview_inspect_layout(ui, &x, &y, &text_w, &btn_x, &btn_w);
    lh = font_line_h();
    preview_inspect_split2(ui->preview_inspect, text_w, line1, sizeof(line1), line2, sizeof(line2));
    font_draw(r, x, y + (UI_BTN_H - lh) / 2, line1, 170, 170, 180);
    if (line2[0]) {
        font_draw(r, x, y + UI_BTN_H + (UI_BTN_H - lh) / 2, line2, 170, 170, 180);
    }
    hover = point_in_rect(ui->mouse_x, ui->mouse_y, btn_x, y, btn_w, UI_BTN_H);
    draw_button(r, btn_x, y, btn_w, "Copy", 1, hover);
}

void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    int plane_bg0 = (ui->worlds_plane == UI_WORLDS_PLANE_BG0);
    const R01Screen *bg1 = NULL;
    const R01Screen *bg0 = NULL;
    screen_origin(ui, &ox, &oy);
    fill_rect(r, ox, oy, ui_screen_w(ui), ui_screen_h(ui), UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!s || !w) {
        font_draw_centered(r, ox, oy, ui_screen_w(ui), ui_screen_h(ui),
                           plane_bg0 ? "No BG0 screen" : "No screen", 160, 160, 170);
        return;
    }
    /*
     * BG1 plane: composite BG0 under BG1 color 0 (emu/hardware preview).
     * BG0 plane: author BG0 alone - do not overlay BG1 or it looks like BG0
     * is drawing BG1 tiles wherever BG1 is opaque.
     */
    if (!ui->hide_bg_layer) {
        if (plane_bg0) {
            bg0 = s;
            bg1 = NULL;
        } else {
            bg1 = s;
            bg0 = r01_world_bg0_screen_at(w, s->col, s->row);
        }
        for (y = 0; y < R01_SCREEN_PX_H; y++) {
            for (x = 0; x < R01_SCREEN_PX_W; x++) {
                uint8_t cr, cg, cb;
                SDL_Rect px;
                r01_compose_screen_pixel_rgb(ui->project, w, bg1, bg0, x, y, &cr, &cg, &cb);
                px.x = ox + x * ui_screen_scale(ui);
                px.y = oy + y * ui_screen_scale(ui);
                px.w = ui_screen_scale(ui);
                px.h = ui_screen_scale(ui);
                SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
                SDL_RenderFillRect(r, &px);
            }
        }
    }
    /* Instance / warp overlays only on BG1 authoring plane. */
    if (!plane_bg0) {
        set_viewport_clip(r, ui, ox, oy);
        if (!ui->hide_spr_layer) {
            draw_instances_on_screen(ui, r, w, s, ox, oy);
        }
        draw_warp_markers(ui, r, w, s, ox, oy);
        SDL_RenderSetClipRect(r, NULL);
    }
    draw_paint_ghost(ui, r, w, ox, oy);
    draw_hover_and_sel_overlays(ui, r, w, s, ox, oy, plane_bg0);
}

static void draw_play_boot(UiState *ui, SDL_Renderer *r, int ox, int oy) {
    static const char spin_chars[] = {'|', '/', '-', '\\'};
    char line[48];
    char spin;
    fill_rect(r, ox, oy, ui_screen_w(ui), ui_screen_h(ui), 0, 0, 0);
    spin = spin_chars[ui->play.spin & 3];
    snprintf(line, sizeof(line), "Booting console... %c", spin);
    font_draw_centered(r, ox, oy + ui_screen_h(ui) / 2 - UI_BTN_H / 2, ui_screen_w(ui), UI_BTN_H, line, 200, 200, 200);
}

static void draw_play_game(UiState *ui, SDL_Renderer *r, int ox, int oy) {
    SDL_Rect dst;
    if (!ui->play.machine || !ui->play.fb_tex) {
        return;
    }
    SDL_UpdateTexture(ui->play.fb_tex, NULL, ui->play.machine->video.fb, R01E_VISIBLE_W * 3);
    dst.x = ox;
    dst.y = oy;
    dst.w = ui_screen_w(ui);
    dst.h = ui_screen_h(ui);
    SDL_RenderCopy(r, ui->play.fb_tex, NULL, &dst);
}

void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox, oy;

    screen_origin(ui, &ox, &oy);
    if (ui->play.booting || !ui->play.machine) {
        draw_play_boot(ui, r, ox, oy);
    } else {
        draw_play_game(ui, r, ox, oy);
    }
}

void draw_catalog_drag_ghost(UiState *ui, SDL_Renderer *r) {
    const R01World *w;
    R01EntityPart pt;
    if (!ui || !ui->catalog_drag.active) {
        return;
    }
    w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    if (!w) {
        return;
    }
    memset(&pt, 0, sizeof(pt));
    if (ui->catalog_drag.active == UI_CATALOG_DRAG_SPRITE) {
        const R01SpriteDef *sp;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= p->sprite_count) {
            return;
        }
        sp = &p->sprites[ui->catalog_drag.index];
        pt.bank = sp->bank;
        pt.tile_id = sp->tile_id;
        pt.pal = sp->pal;
        draw_spr_tile_px(ui, r, w, &pt, ui->mouse_x - ui->catalog_drag.off_x, ui->mouse_y - ui->catalog_drag.off_y,
                         0, 0, 1, 0);
    } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_METASPRITE) {
        const R01MetaspriteDef *ms;
        int i, gx, gy;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= p->metasprite_count) {
            return;
        }
        ms = &p->metasprites[ui->catalog_drag.index];
        gx = ui->mouse_x - ui->catalog_drag.off_x;
        gy = ui->mouse_y - ui->catalog_drag.off_y;
        for (i = 0; i < ms->frame.part_count; i++) {
            draw_spr_tile_px(ui, r, w, &ms->frame.parts[i], gx + ms->frame.parts[i].dx, gy + ms->frame.parts[i].dy, 0,
                             0, 1, 0);
        }
    } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_ENTITY) {
        const R01EntityType *ent;
        const R01EntityFrame *fr = NULL;
        int si, fi, pi;
        int gx, gy;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= p->entity_count) {
            return;
        }
        ent = &p->entities[ui->catalog_drag.index];
        for (si = 0; si < ent->state_count && !fr; si++) {
            const R01EntityState *st = &ent->states[si];
            for (fi = 0; fi < st->frame_count; fi++) {
                if (st->frames[fi].part_count > 0) {
                    fr = &st->frames[fi];
                    break;
                }
            }
        }
        if (!fr) {
            return;
        }
        gx = ui->mouse_x - ui->catalog_drag.off_x;
        gy = ui->mouse_y - ui->catalog_drag.off_y;
        for (pi = 0; pi < fr->part_count; pi++) {
            const R01EntityPart *part = &fr->parts[pi];
            draw_spr_tile_px(ui, r, w, part, gx + part->dx - fr->origin_x, gy + part->dy - fr->origin_y, 0, 0, 1,
                             0);
        }
    } else {
        return;
    }
}
