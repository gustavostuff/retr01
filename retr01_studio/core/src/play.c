#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/game_runtime.h"
#include "retr01_studio/play.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/warps.h"

#include <string.h>

#define R01_PROJ_FIXED_SHIFT 8

static void place_player_on_screen(R01PlayState *pl, int col, int row) {
    r01_player_warp(&pl->ctx, col, row);
}

static void place_player_xy(R01PlayState *pl, int wx, int wy) {
    pl->ctx.player_x = wx;
    pl->ctx.player_y = wy;
    r01_game_camera_update(&pl->ctx);
}

static int play_spawn_screen(const R01World *w, int *out_col, int *out_row) {
    int idx;
    if (!w) {
        return 0;
    }
    idx = r01_world_default_screen(w);
    if (idx < 0 || idx >= w->screen_count || !w->screens[idx].present) {
        return 0;
    }
    if (out_col) {
        *out_col = w->screens[idx].col;
    }
    if (out_row) {
        *out_row = w->screens[idx].row;
    }
    return 1;
}

static int play_player_instance_spawn(const R01World *w, int *out_x, int *out_y) {
    int pe;
    int i;
    if (!w) {
        return 0;
    }
    pe = r01_world_player_entity(w);
    if (pe < 0) {
        return 0;
    }
    for (i = 0; i < w->instance_count; i++) {
        if (w->instances[i].type_id != pe) {
            continue;
        }
        if (out_x) {
            *out_x = w->instances[i].world_x;
        }
        if (out_y) {
            *out_y = w->instances[i].world_y;
        }
        return 1;
    }
    return 0;
}

int r01_play_start(R01PlayState *pl, const R01Project *p) {
    const R01World *w;
    int col = 0, row = 0;
    int sx, sy;
    if (!pl) {
        return 0;
    }
    memset(pl, 0, sizeof(*pl));
    r01_game_ctx_init(&pl->ctx);
    if (!p) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return 0;
    }
    pl->active = 1;
    if (play_player_instance_spawn(w, &sx, &sy)) {
        place_player_xy(pl, sx, sy);
        return 1;
    }
    if (!play_spawn_screen(w, &col, &row)) {
        pl->active = 0;
        return 0;
    }
    place_player_on_screen(pl, col, row);
    return 1;
}

void r01_play_stop(R01PlayState *pl) {
    if (pl) {
        pl->active = 0;
    }
}

void r01_play_player_hit_rect(const R01World *w, int origin_x, int origin_y, int *hx, int *hy, int *hw,
                              int *hh) {
    int pe;
    int box_w = R01_PLAY_PLAYER_W;
    int box_h = R01_PLAY_PLAYER_H;
    int box_x = origin_x;
    int box_y = origin_y;
    pe = r01_world_player_entity(w);
    if (pe >= 0 && w->entities[pe].state_count > 0) {
        const R01EntityState *st = &w->entities[pe].states[0];
        box_x = r01_entity_world_x(origin_x, st->origin_x, st->hitbox_x);
        box_y = r01_entity_world_y(origin_y, st->origin_y, st->hitbox_y);
        if (st->hitbox_w > 0) {
            box_w = st->hitbox_w;
        }
        if (st->hitbox_h > 0) {
            box_h = st->hitbox_h;
        }
    }
    if (hx) {
        *hx = box_x;
    }
    if (hy) {
        *hy = box_y;
    }
    if (hw) {
        *hw = box_w;
    }
    if (hh) {
        *hh = box_h;
    }
}

void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy) {
    const R01World *w;
    R01GameCtx *ctx;
    if (!pl || !pl->active || !p) {
        return;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return;
    }
    ctx = &pl->ctx;
    if (r01_game_fade_active(ctx)) {
        if (r01_game_fade_tick(ctx)) {
            r01_game_fade_warp_step(ctx, w);
        }
        return;
    }
    if (dx != 0) {
        int nx = ctx->player_x + dx;
        int hx, hy, hw, hh;
        r01_play_player_hit_rect(w, nx, ctx->player_y, &hx, &hy, &hw, &hh);
        if (r01_world_aabb_ok(w, hx, hy, hw, hh)) {
            ctx->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = ctx->player_y + dy;
        int hx, hy, hw, hh;
        r01_play_player_hit_rect(w, ctx->player_x, ny, &hx, &hy, &hw, &hh);
        if (r01_world_aabb_ok(w, hx, hy, hw, hh)) {
            ctx->player_y = ny;
        }
    }
    r01_game_camera_update(ctx);
    r01_projectile_tick(ctx, w);
    r01_game_warp_check(ctx, w);
    if (r01_game_fade_active(ctx)) {
        r01_game_fade_tick(ctx);
    }
}

int r01_play_button(R01PlayState *pl, const R01Project *p, int button) {
    if (button == R01_PLAY_BTN_X) {
        r01_player_warp(&pl->ctx, 0, 0);
        return 1;
    }
    if (button == R01_PLAY_BTN_Y) {
        r01_player_warp(&pl->ctx, 1, 0);
        return 1;
    }
    return 0;
}

int r01_play_screen_index(const R01PlayState *pl, const R01World *w) {
    int col, row;
    if (!pl || !w) {
        return -1;
    }
    col = (pl->ctx.player_x + R01_PLAY_PLAYER_W / 2) / R01_SCREEN_PX_W;
    row = (pl->ctx.player_y + R01_PLAY_PLAYER_H / 2) / R01_SCREEN_PX_H;
    return r01_world_find_screen(w, col, row);
}

int r01_play_fade_level(const R01PlayState *pl) {
    return pl ? pl->ctx.fade_level : 0;
}

int r01_play_fade_color(const R01PlayState *pl) {
    return pl ? pl->ctx.fade_color : R01_FADE_BLACK;
}

int r01_play_sample_bg(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                       uint8_t *b) {
    const R01World *w;
    int wx, wy, col, row, idx;
    const R01Screen *s;
    if (!p || !pl || vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H) {
        return -1;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return -1;
    }
    wx = pl->ctx.cam_x + vx;
    wy = pl->ctx.cam_y + vy;
    if (wx < 0 || wy < 0) {
        r01_project_backdrop_rgb(p, w, r, g, b);
        return 0;
    }
    col = wx / R01_SCREEN_PX_W;
    row = wy / R01_SCREEN_PX_H;
    idx = r01_world_find_screen(w, col, row);
    if (idx < 0) {
        r01_project_backdrop_rgb(p, w, r, g, b);
        return 0;
    }
    s = &w->screens[idx];
    r01_screen_pixel_rgb(p, w, s, wx % R01_SCREEN_PX_W, wy % R01_SCREEN_PX_H, r, g, b);
    return 0;
}

int r01_play_build_oam(const R01Project *p, const R01PlayState *pl, R01OamEntry *out, int cap) {
    const R01World *w;
    const R01GameCtx *ctx;
    int n = 0;
    int i;
    int player_type;
    if (!p || !pl || !out || cap < 1) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    ctx = &pl->ctx;
    if (!w) {
        return 0;
    }

    player_type = r01_world_player_entity(w);
    if (player_type >= 0) {
        const R01EntityType *ent = &w->entities[player_type];
        const R01EntityState *st;
        const R01EntityFrame *fr;
        int pi;
        if (ent->state_count > 0 && ent->states[0].frame_count > 0) {
            st = &ent->states[0];
            fr = &st->frames[0];
            for (pi = 0; pi < fr->part_count && n < cap && n < R01_OAM_MAX; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                int dx, dy, fh, fv;
                int ox, oy;
                r01_entity_part_instance_pose(st, pt, 0, 0, &dx, &dy, &fh, &fv);
                ox = r01_entity_world_x(ctx->player_x, st->origin_x, dx) - ctx->cam_x;
                oy = r01_entity_world_y(ctx->player_y, st->origin_y, dy) - ctx->cam_y;
                if (r01_oam_tile_off_screen(ox, oy)) {
                    continue;
                }
                out[n].x = ox;
                out[n].y = oy;
                out[n].bank = pt->bank;
                out[n].tile_id = pt->tile_id;
                out[n].pal = pt->pal;
                out[n].flip_h = fh;
                out[n].flip_v = fv;
                n++;
            }
        }
    }
    if (n < 1) {
        out[0].x = ctx->player_x - ctx->cam_x;
        out[0].y = ctx->player_y - ctx->cam_y;
        out[0].bank = 0;
        out[0].tile_id = R01_SPR_PLAYER_TILE_ID;
        out[0].pal = 0;
        out[0].flip_h = 0;
        out[0].flip_v = 0;
        n = 1;
    }

    for (i = 0; i < R01_MAX_PROJECTILES && n < cap && n < R01_OAM_MAX; i++) {
        const R01Projectile *pr = &ctx->projectiles[i];
        int ox, oy;
        if (!pr->active) {
            continue;
        }
        ox = (pr->x >> R01_PROJ_FIXED_SHIFT) - ctx->cam_x;
        oy = (pr->y >> R01_PROJ_FIXED_SHIFT) - ctx->cam_y;
        if (r01_oam_tile_off_screen(ox, oy)) {
            continue;
        }
        out[n].x = ox;
        out[n].y = oy;
        out[n].bank = 0;
        out[n].tile_id = pr->tile;
        out[n].pal = pr->pal;
        out[n].flip_h = 0;
        out[n].flip_v = 0;
        n++;
    }

    for (i = 0; i < w->instance_count && n < cap && n < R01_OAM_MAX; i++) {
        const R01EntityInstance *inst = &w->instances[i];
        const R01EntityType *ent;
        const R01EntityState *st;
        const R01EntityFrame *fr;
        int pi;
        if (inst->type_id < 0 || inst->type_id >= w->entity_count) {
            continue;
        }
        if (player_type >= 0 && inst->type_id == player_type) {
            continue;
        }
        ent = &w->entities[inst->type_id];
        if (ent->state_count < 1 || ent->states[0].frame_count < 1) {
            continue;
        }
        st = &ent->states[0];
        fr = &st->frames[0];
        for (pi = 0; pi < fr->part_count && n < cap && n < R01_OAM_MAX; pi++) {
            const R01EntityPart *pt = &fr->parts[pi];
            int dx, dy, fh, fv;
            int ox, oy;
            r01_entity_part_instance_pose(st, pt, inst->flip_h, inst->flip_v, &dx, &dy, &fh, &fv);
            ox = r01_entity_world_x(inst->world_x, st->origin_x, dx) - ctx->cam_x;
            oy = r01_entity_world_y(inst->world_y, st->origin_y, dy) - ctx->cam_y;
            if (r01_oam_tile_off_screen(ox, oy)) {
                continue;
            }
            out[n].x = ox;
            out[n].y = oy;
            out[n].bank = pt->bank;
            out[n].tile_id = pt->tile_id;
            out[n].pal = pt->pal;
            out[n].flip_h = fh;
            out[n].flip_v = fv;
            n++;
        }
    }
    return n;
}
