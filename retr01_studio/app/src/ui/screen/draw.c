#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_spr_tile_px(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityPart *pt, int dx,
                             int dy, int scale) {
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
            SDL_Rect px;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            px.x = dx + sx * scale;
            px.y = dy + sy * scale;
            px.w = scale;
            px.h = scale;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
}

static void draw_entity_at_screen(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityType *ent,
                                  int local_x, int local_y, int ox, int oy, int selected) {
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
        int px = r01_entity_world_x(local_x, st->origin_x, pt->dx);
        int py = r01_entity_world_y(local_y, st->origin_y, pt->dy);
        draw_spr_tile_px(ui, r, w, pt, ox + px * UI_SCREEN_SCALE, oy + py * UI_SCREEN_SCALE, UI_SCREEN_SCALE);
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
        draw_rect(r, ox + min_x * UI_SCREEN_SCALE, oy + min_y * UI_SCREEN_SCALE,
                  (max_x - min_x) * UI_SCREEN_SCALE, (max_y - min_y) * UI_SCREEN_SCALE, 255, 255, 255);
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
        if (inst->type_id < 0 || inst->type_id >= w->entity_count) {
            continue;
        }
        if (local_x < -R01_ENTITY_COMPOSE_PX || local_y < -R01_ENTITY_COMPOSE_PX ||
            local_x >= R01_SCREEN_PX_W + R01_ENTITY_COMPOSE_PX ||
            local_y >= R01_SCREEN_PX_H + R01_ENTITY_COMPOSE_PX) {
            continue;
        }
        ent = &w->entities[inst->type_id];
        draw_entity_at_screen(ui, r, w, ent, local_x, local_y, ox, oy, i == ui->sel_instance);
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
            int part_x = r01_entity_world_x(local_x, st->origin_x, pt->dx);
            int part_y = r01_entity_world_y(local_y, st->origin_y, pt->dy);
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

void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
    screen_origin(ui, &ox, &oy);
    fill_rect(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!s || !w) {
        font_draw_centered(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, "No screen", 160, 160, 170);
        return;
    }
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t cr, cg, cb;
            SDL_Rect px;
            r01_screen_pixel_rgb(ui->project, w, s, x, y, &cr, &cg, &cb);
            px.x = ox + x * UI_SCREEN_SCALE;
            px.y = oy + y * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    draw_instances_on_screen(ui, r, w, s, ox, oy);
    if (screen_sel_valid(ui) && ui->screen_mode == UI_SCREEN_MODE_SEL && ui->sel_instance < 0) {
        int min_x, min_y, max_x, max_y;
        int sx, sy, sw, sh;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        sx = ox + min_x * 8 * UI_SCREEN_SCALE;
        sy = oy + min_y * 8 * UI_SCREEN_SCALE;
        sw = (max_x - min_x + 1) * 8 * UI_SCREEN_SCALE;
        sh = (max_y - min_y + 1) * 8 * UI_SCREEN_SCALE;
        draw_rect(r, sx, sy, sw, sh, 255, 255, 255);
    }
}

static void draw_oam_sprites(UiState *ui, SDL_Renderer *r, int ox, int oy) {
    R01OamEntry oam[R01_OAM_MAX];
    const R01World *w = r01_project_active_world_const(ui->project);
    int n = r01_play_build_oam(ui->project, &ui->play, oam, R01_OAM_MAX);
    int i;
    if (!w) {
        return;
    }
    for (i = 0; i < n; i++) {
        R01EntityPart pt;
        if (i == 0) {
            int pcx, pcy;
            uint8_t pr, pg, pb;
            r01_project_player_rgb(ui->project, &pr, &pg, &pb);
            for (pcy = 0; pcy < R01_PLAY_PLAYER_H; pcy++) {
                for (pcx = 0; pcx < R01_PLAY_PLAYER_W; pcx++) {
                    int vx = oam[0].x + pcx;
                    int vy = oam[0].y + pcy;
                    SDL_Rect px;
                    if (vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H) {
                        continue;
                    }
                    px.x = ox + vx * UI_SCREEN_SCALE;
                    px.y = oy + vy * UI_SCREEN_SCALE;
                    px.w = UI_SCREEN_SCALE;
                    px.h = UI_SCREEN_SCALE;
                    SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
                    SDL_RenderFillRect(r, &px);
                }
            }
            continue;
        }
        memset(&pt, 0, sizeof(pt));
        pt.bank = oam[i].bank;
        pt.tile_id = oam[i].tile_id;
        pt.pal = oam[i].pal;
        pt.flip_h = oam[i].flip_h;
        pt.flip_v = oam[i].flip_v;
        if (oam[i].x + 8 <= 0 || oam[i].y + 8 <= 0 || oam[i].x >= R01_SCREEN_PX_W ||
            oam[i].y >= R01_SCREEN_PX_H) {
            continue;
        }
        draw_spr_tile_px(ui, r, w, &pt, ox + oam[i].x * UI_SCREEN_SCALE, oy + oam[i].y * UI_SCREEN_SCALE,
                         UI_SCREEN_SCALE);
    }
}

void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox, oy, vy, vx;
    screen_origin(ui, &ox, &oy);
    for (vy = 0; vy < R01_SCREEN_PX_H; vy++) {
        for (vx = 0; vx < R01_SCREEN_PX_W; vx++) {
            uint8_t cr = 0, cg = 0, cb = 0;
            SDL_Rect px;
            r01_play_sample_bg(ui->project, &ui->play, vx, vy, &cr, &cg, &cb);
            px.x = ox + vx * UI_SCREEN_SCALE;
            px.y = oy + vy * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    draw_oam_sprites(ui, r, ox, oy);
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
    } else {
        return;
    }
    draw_spr_tile_px(ui, r, w, &pt, ui->mouse_x - ui->catalog_drag.off_x, ui->mouse_y - ui->catalog_drag.off_y,
                     UI_SCREEN_SCALE);
}
