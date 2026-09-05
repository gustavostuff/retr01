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
    raw = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
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
        r01_entity_part_instance_pose(st, pt, flip_h, flip_v, &dx, &dy, NULL, NULL);
        px = r01_entity_world_x(local_x, st->origin_x, dx);
        py = r01_entity_world_y(local_y, st->origin_y, dy);
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
        r01_entity_part_instance_pose(st, pt, flip_h, flip_v, &dx, &dy, &fh, &fv);
        px = r01_entity_world_x(local_x, st->origin_x, dx);
        py = r01_entity_world_y(local_y, st->origin_y, dy);
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
        int sel_x = min_x;
        int sel_y = min_y;
        int sel_w = max_x - min_x;
        int sel_h = max_y - min_y;
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
            draw_rect(r, ox + sel_x * ui_screen_scale(ui), oy + sel_y * ui_screen_scale(ui),
                      sel_w * ui_screen_scale(ui), sel_h * ui_screen_scale(ui), 255, 255, 255);
        }
    }
}

static void draw_instances_on_screen(UiState *ui, SDL_Renderer *r, const R01World *w, const R01Screen *s, int ox,
                                     int oy) {
    int i;
    if (!w || !s) {
        return;
    }
    for (i = 0; i < w->instance_count; i++) {
        const R01EntityInstance *inst = &w->instances[i];
        const R01EntityType *ent;
        int local_x = inst->world_x - s->col * R01_SCREEN_PX_W;
        int local_y = inst->world_y - s->row * R01_SCREEN_PX_H;
        int min_x, min_y, max_x, max_y;
        if (inst->type_id < 0 || inst->type_id >= w->entity_count) {
            continue;
        }
        ent = &w->entities[inst->type_id];
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
    if (!ui || ui->play.active) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
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
        if (inst->type_id < 0 || inst->type_id >= w->entity_count) {
            continue;
        }
        ent = &w->entities[inst->type_id];
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
            r01_entity_part_instance_pose(st, pt, inst->flip_h, inst->flip_v, &dx, &dy, NULL, NULL);
            part_x = r01_entity_world_x(local_x, st->origin_x, dx);
            part_y = r01_entity_world_y(local_y, st->origin_y, dy);
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

void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
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
     * Composite BG0 under BG1 color 0 (same as emu/hardware). Same absolute
     * chess cell; orphan BG0 outside the BG1 present bbox is ignored when
     * viewing BG1 (no underlay). Viewing BG0 alone still composites if a
     * present BG1 screen shares that cell.
     */
    if (plane_bg0) {
        bg0 = s;
        bg1 = r01_world_screen_at(w, s->col, s->row);
        if (bg1 && !bg1->present) {
            bg1 = NULL;
        }
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
    /* Instance / warp overlays only on BG1 authoring plane. */
    if (!plane_bg0) {
        set_viewport_clip(r, ui, ox, oy);
        draw_instances_on_screen(ui, r, w, s, ox, oy);
        draw_warp_markers(ui, r, w, s, ox, oy);
        SDL_RenderSetClipRect(r, NULL);
    }
    if (screen_sel_valid(ui) && ui->screen_mode == UI_SCREEN_MODE_SEL && ui->sel_instance < 0) {
        int min_x, min_y, max_x, max_y;
        int sx, sy, sw, sh;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        sx = ox + min_x * 8 * ui_screen_scale(ui);
        sy = oy + min_y * 8 * ui_screen_scale(ui);
        sw = (max_x - min_x + 1) * 8 * ui_screen_scale(ui);
        sh = (max_y - min_y + 1) * 8 * ui_screen_scale(ui);
        draw_rect(r, sx, sy, sw, sh, 255, 255, 255);
    }
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
    if (!w) {
        return;
    }
    memset(&pt, 0, sizeof(pt));
    if (ui->catalog_drag.active == UI_CATALOG_DRAG_SPRITE) {
        const R01SpriteDef *sp;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= w->sprite_count) {
            return;
        }
        sp = &w->sprites[ui->catalog_drag.index];
        pt.bank = sp->bank;
        pt.tile_id = sp->tile_id;
        pt.pal = sp->pal;
        draw_spr_tile_px(ui, r, w, &pt, ui->mouse_x - ui->catalog_drag.off_x, ui->mouse_y - ui->catalog_drag.off_y,
                         0, 0, 1, 0);
    } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_METASPRITE) {
        const R01MetaspriteDef *ms;
        int i, gx, gy;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= w->metasprite_count) {
            return;
        }
        ms = &w->metasprites[ui->catalog_drag.index];
        gx = ui->mouse_x - ui->catalog_drag.off_x;
        gy = ui->mouse_y - ui->catalog_drag.off_y;
        for (i = 0; i < ms->frame.part_count; i++) {
            draw_spr_tile_px(ui, r, w, &ms->frame.parts[i], gx + ms->frame.parts[i].dx, gy + ms->frame.parts[i].dy, 0,
                             0, 1, 0);
        }
    } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_ENTITY) {
        const R01EntityType *ent;
        if (ui->catalog_drag.index < 0 || ui->catalog_drag.index >= w->entity_count) {
            return;
        }
        ent = &w->entities[ui->catalog_drag.index];
        if (ent->state_count < 1 || ent->states[0].frame_count < 1 ||
            ent->states[0].frames[0].part_count < 1) {
            return;
        }
        pt = ent->states[0].frames[0].parts[0];
        draw_spr_tile_px(ui, r, w, &pt, ui->mouse_x - ui->catalog_drag.off_x, ui->mouse_y - ui->catalog_drag.off_y,
                         0, 0, 1, 0);
    } else {
        return;
    }
}
