#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/game_runtime.h"
#include "retr01_studio/play.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/player_anim.h"
#include "retr01_studio/paths.h"
#include "r01_custom_logic_scan.h"
#include "r01_play_physics.h"

#include <stdio.h>
#include <string.h>

#define R01_PROJ_FIXED_SHIFT 8

static void play_apply_custom_logic(R01GameCtx *ctx, const char *project_path) {
    char path[R01_PATH_MAX];
    int dx;
    int dy;
    int mode;
    int grav;
    int jump;
    int meter;
    int idle = -1;
    int walk = -1;
    int jump_state = -1;
    int crouch;
    if (!ctx) {
        return;
    }
    if (project_path && project_path[0]) {
        if (r01_custom_logic_path_for_project(project_path, path, sizeof(path)) != 0) {
            return;
        }
    } else if (r01_path_resolve("output/C/custom_logic.c", path, sizeof(path)) != 0) {
        snprintf(path, sizeof(path), "output/C/custom_logic.c");
    }
    if (r01_custom_logic_scan_deadzone(path, &dx, &dy) == 0) {
        r01_camera_set_deadzone(ctx, dx, dy);
    }
    if (r01_custom_logic_scan_game_mode(path, &mode) == 0) {
        r01_game_set_mode(ctx, mode);
    }
    if (r01_custom_logic_scan_plat_gravity(path, &grav) == 0 && grav > 0) {
        r01_platformer_set_gravity(ctx, grav);
    }
    if (r01_custom_logic_scan_plat_jump(path, &jump) == 0 && jump > 0) {
        r01_platformer_set_jump(ctx, jump);
    }
    if (r01_custom_logic_scan_plat_meter(path, &meter) == 0 && meter > 0) {
        r01_platformer_set_meter(ctx, meter);
    }
    if (r01_custom_logic_scan_player_idle(path, &idle) == 0) {
        r01_player_anim_set_idle_state(ctx, idle);
    }
    if (r01_custom_logic_scan_player_walk(path, &walk) == 0) {
        r01_player_anim_set_walk_all(ctx, walk);
    }
    if (r01_custom_logic_scan_plat_crouch(path, &crouch) == 0) {
        r01_player_anim_set_crouch_state(ctx, crouch);
    }
    if (r01_custom_logic_scan_player_jump(path, &jump_state) == 0) {
        r01_player_anim_set_jump_state(ctx, jump_state);
    }
    if (r01_custom_logic_scan_run_on_x(path) == 0) {
        r01_player_set_run_on_x(ctx);
    }
}

static void place_player_on_screen(R01PlayState *pl, int col, int row) {
    r01_player_warp(&pl->ctx, col, row);
}

static void place_player_xy(R01PlayState *pl, int wx, int wy) {
    pl->ctx.player_x = wx;
    pl->ctx.player_y = wy;
    pl->ctx.plat_vel_y = 0;
    pl->ctx.plat_frac_x = 0;
    pl->ctx.plat_frac_y = 0;
    pl->ctx.plat_grounded = 0;
    pl->ctx.plat_jump_held = 0;
    r01_game_camera_snap(&pl->ctx);
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

static int play_player_instance_spawn(const R01Project *p, const R01World *w, int *out_x, int *out_y) {
    int pe;
    int i;
    if (!p || !w) {
        return 0;
    }
    pe = r01_world_player_entity(p);
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

int r01_play_start(R01PlayState *pl, const R01Project *p, const char *project_path) {
    const R01World *w;
    int col = 0, row = 0;
    int sx, sy;
    if (!pl) {
        return 0;
    }
    memset(pl, 0, sizeof(*pl));
    r01_game_ctx_init(&pl->ctx);
    play_apply_custom_logic(&pl->ctx, project_path);
    if (!p) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return 0;
    }
    pl->active = 1;
    if (play_player_instance_spawn(p, w, &sx, &sy)) {
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

void r01_play_player_hit_rect(const R01Project *p, const R01GameCtx *ctx, int origin_x, int origin_y, int *hx,
                              int *hy, int *hw, int *hh) {
    int pe;
    int state_idx = 0;
    int box_w = R01_PLAY_PLAYER_W;
    int box_h = R01_PLAY_PLAYER_H;
    int box_x = origin_x;
    int box_y = origin_y;
    pe = r01_world_player_entity(p);
    if (ctx) {
        state_idx = r01_player_anim_entity_state(ctx);
    }
    if (p && pe >= 0 && p->entities[pe].state_count > 0) {
        const R01EntityState *st;
        const R01EntityFrame *fr;
        if (state_idx < 0 || state_idx >= p->entities[pe].state_count) {
            state_idx = 0;
        }
        st = &p->entities[pe].states[state_idx];
        fr = r01_entity_state_hitbox_origin_frame(st);
        if (fr) {
            box_x = r01_entity_world_x(origin_x, fr->origin_x, st->hitbox_x);
            box_y = r01_entity_world_y(origin_y, fr->origin_y, st->hitbox_y);
            if (st->hitbox_w > 0) {
                box_w = st->hitbox_w;
            }
            if (st->hitbox_h > 0) {
                box_h = st->hitbox_h;
            }
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

typedef struct PlayMoveCtx {
    const R01Project *p;
    const R01World *w;
    const R01GameCtx *ctx;
} PlayMoveCtx;

static int play_move_ok(void *user, int ox, int oy) {
    PlayMoveCtx *m = (PlayMoveCtx *)user;
    int hx, hy, hw, hh;
    if (!m || !m->w) {
        return 0;
    }
    r01_play_player_hit_rect(m->p, m->ctx, ox, oy, &hx, &hy, &hw, &hh);
    return r01_world_aabb_ok(m->w, hx, hy, hw, hh);
}

void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy, int jump_down) {
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
    {
        int pe = r01_world_player_entity(p);
        int anim_dx = 0;
        int anim_dy = 0;
        PlayMoveCtx move;
        R01PlayPhysics ph;
        r01_play_physics_init(&ph);
        r01_play_physics_set_mode(&ph, ctx->game_mode);
        r01_play_physics_set_gravity(&ph, ctx->plat_gravity);
        r01_play_physics_set_jump(&ph, ctx->plat_jump);
        r01_play_physics_set_meter(&ph, ctx->plat_meter);
        {
            int run = (ctx->player_run_on_x && r01_pad_pressed(ctx, R01_BTN_X)) ? 2 : 1;
            r01_play_physics_set_run_mul(&ph, run);
            ctx->player_run_fast = (run == 2);
        }
        ph.vel_y = ctx->plat_vel_y;
        ph.frac_x = ctx->plat_frac_x;
        ph.frac_y = ctx->plat_frac_y;
        ph.grounded = ctx->plat_grounded;
        ph.jump_held = ctx->plat_jump_held;
        move.p = p;
        move.w = w;
        move.ctx = ctx;
        {
            int crouch = 0;
            int phys_dx = dx;
            if (ctx->game_mode == R01_GAME_MODE_PLATFORMER && ctx->plat_grounded && dy > 0 &&
                ctx->player_crouch_state >= 0) {
                crouch = 1;
                phys_dx = 0;
            }
            r01_play_physics_tick(&ph, &ctx->player_x, &ctx->player_y, phys_dx, dy, jump_down, play_move_ok,
                                  &move, &anim_dx, &anim_dy);
            ctx->player_crouching = crouch;
        }
        ctx->plat_vel_y = ph.vel_y;
        ctx->plat_frac_x = ph.frac_x;
        ctx->plat_frac_y = ph.frac_y;
        ctx->plat_grounded = ph.grounded;
        ctx->plat_jump_held = ph.jump_held;
        ctx->player_airborne = (ctx->game_mode == R01_GAME_MODE_PLATFORMER && !ph.grounded) ? 1 : 0;
        if (ctx->player_airborne) {
            ctx->player_crouching = 0;
        }
        r01_player_anim_update(ctx, anim_dx, anim_dy);
        r01_player_anim_tick(ctx, p, pe);
    }
    r01_game_camera_update(ctx);
    r01_projectile_tick(ctx, w);
    r01_game_warp_check(ctx, w);
    if (r01_game_fade_active(ctx)) {
        r01_game_fade_tick(ctx);
    }
}

int r01_play_button(R01PlayState *pl, const R01Project *p, int button) {
    (void)pl;
    (void)p;
    (void)button;
    return 0;
}

int r01_play_screen_index(const R01PlayState *pl, const R01World *w) {
    if (!pl || !w) {
        return -1;
    }
    return r01_world_find_screen_overlapping(w, pl->ctx.player_x, pl->ctx.player_y, R01_PLAY_PLAYER_W,
                                             R01_PLAY_PLAYER_H);
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

    player_type = r01_world_player_entity(p);
    if (player_type >= 0) {
        const R01EntityType *ent = &p->entities[player_type];
        const R01EntityState *st;
        const R01EntityFrame *fr;
        int state_idx = r01_player_anim_entity_state(ctx);
        int frame_idx = r01_player_anim_frame(ctx);
        int flip_h = r01_player_anim_flip_h(ctx);
        int pi;
        if (state_idx < 0 || state_idx >= ent->state_count) {
            state_idx = 0;
        }
        if (ent->state_count > 0 && ent->states[state_idx].frame_count > 0) {
            st = &ent->states[state_idx];
            frame_idx = r01_entity_state_drawable_frame_index(st, frame_idx);
            fr = &st->frames[frame_idx];
            for (pi = 0; pi < fr->part_count && n < cap && n < R01_OAM_MAX; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                int dx, dy, fh, fv;
                int ox, oy;
                r01_entity_part_instance_pose(fr, pt, flip_h, 0, &dx, &dy, &fh, &fv);
                ox = r01_entity_world_x(ctx->player_x, fr->origin_x, dx) - ctx->cam_x;
                oy = r01_entity_world_y(ctx->player_y, fr->origin_y, dy) - ctx->cam_y;
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
        if (inst->type_id < 0 || inst->type_id >= p->entity_count) {
            continue;
        }
        if (player_type >= 0 && inst->type_id == player_type) {
            continue;
        }
        ent = &p->entities[inst->type_id];
        if (ent->state_count < 1 || ent->states[0].frame_count < 1) {
            continue;
        }
        st = &ent->states[0];
        fr = &st->frames[0];
        for (pi = 0; pi < fr->part_count && n < cap && n < R01_OAM_MAX; pi++) {
            const R01EntityPart *pt = &fr->parts[pi];
            int dx, dy, fh, fv;
            int ox, oy;
            r01_entity_part_instance_pose(fr, pt, inst->flip_h, inst->flip_v, &dx, &dy, &fh, &fv);
            ox = r01_entity_world_x(inst->world_x, fr->origin_x, dx) - ctx->cam_x;
            oy = r01_entity_world_y(inst->world_y, fr->origin_y, dy) - ctx->cam_y;
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
