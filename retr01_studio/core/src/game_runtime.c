#include "retr01_studio/game_runtime.h"
#include "retr01_studio/player_anim.h"
#include "retr01_studio/collision.h"
#include "retr01_studio/play.h"
#include "retr01_studio/warps.h"

#include <math.h>
#include <string.h>

#define R01_EVENT_SLOTS 4
#define R01_PROJ_SPEED_DEFAULT 3
#define R01_PROJ_FIXED_SHIFT 8

static struct {
    uint8_t btn;
    R01EventFn fn;
} s_events[R01_EVENT_SLOTS];

void r01_game_ctx_init(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->cam_deadzone_x = 0;
    ctx->cam_deadzone_y = 0;
    ctx->cam_axis_lock = R01_CAM_AXIS_BOTH;
    ctx->fade_color = R01_FADE_BLACK;
    ctx->fade_pending_entrance = -1;
    r01_player_anim_init(ctx);
}

void r01_game_camera_update(R01GameCtx *ctx) {
    int ax, ay, target_x, target_y;
    if (!ctx) {
        return;
    }
    ax = ctx->player_x + R01_PLAY_PLAYER_W / 2;
    ay = ctx->player_y + R01_PLAY_PLAYER_H / 2;
    target_x = ax - R01_SCREEN_PX_W / 2;
    target_y = ay - R01_SCREEN_PX_H / 2;
    if (ctx->cam_axis_lock != R01_CAM_AXIS_V) {
        if (ax - ctx->cam_x < ctx->cam_deadzone_x) {
            ctx->cam_x = ax - ctx->cam_deadzone_x;
        } else if (ax - ctx->cam_x > R01_SCREEN_PX_W - ctx->cam_deadzone_x - R01_PLAY_PLAYER_W) {
            ctx->cam_x = ax - (R01_SCREEN_PX_W - ctx->cam_deadzone_x - R01_PLAY_PLAYER_W);
        }
        if (ctx->cam_deadzone_x <= 0) {
            ctx->cam_x = target_x;
        }
    }
    if (ctx->cam_axis_lock != R01_CAM_AXIS_H) {
        if (ay - ctx->cam_y < ctx->cam_deadzone_y) {
            ctx->cam_y = ay - ctx->cam_deadzone_y;
        } else if (ay - ctx->cam_y > R01_SCREEN_PX_H - ctx->cam_deadzone_y - R01_PLAY_PLAYER_H) {
            ctx->cam_y = ay - (R01_SCREEN_PX_H - ctx->cam_deadzone_y - R01_PLAY_PLAYER_H);
        }
        if (ctx->cam_deadzone_y <= 0) {
            ctx->cam_y = target_y;
        }
    }
    if (ctx->cam_x < 0) {
        ctx->cam_x = 0;
    }
    if (ctx->cam_y < 0) {
        ctx->cam_y = 0;
    }
}

void r01_game_fade_start(R01GameCtx *ctx, int to_black_or_white, int target_level) {
    if (!ctx) {
        return;
    }
    if (target_level < 0) {
        target_level = 0;
    }
    if (target_level > R01_FADE_MAX) {
        target_level = R01_FADE_MAX;
    }
    ctx->fade_color = to_black_or_white ? R01_FADE_WHITE : R01_FADE_BLACK;
    ctx->fade_target = target_level;
}

int r01_game_fade_active(const R01GameCtx *ctx) {
    return ctx && ctx->fade_level != ctx->fade_target;
}

int r01_game_fade_tick(R01GameCtx *ctx) {
    if (!ctx) {
        return 0;
    }
    if (ctx->fade_level < ctx->fade_target) {
        ctx->fade_level += R01_FADE_SPEED;
        if (ctx->fade_level > ctx->fade_target) {
            ctx->fade_level = ctx->fade_target;
        }
    } else if (ctx->fade_level > ctx->fade_target) {
        ctx->fade_level -= R01_FADE_SPEED;
        if (ctx->fade_level < ctx->fade_target) {
            ctx->fade_level = ctx->fade_target;
        }
    }
    return ctx->fade_level == ctx->fade_target;
}

void r01_game_warp_to_tile(R01GameCtx *ctx, int screen_col, int screen_row, int tile_col,
                           int tile_row) {
    if (!ctx) {
        return;
    }
    r01_world_warp_tile_world_pos(screen_col, screen_row, tile_col, tile_row, &ctx->player_x,
                                  &ctx->player_y);
    r01_game_camera_update(ctx);
}

static void warp_execute_exit(R01GameCtx *ctx, const R01World *w, int entrance_idx) {
    int exit_idx = r01_world_warp_exit_for_entrance(w, entrance_idx);
    const R01WarpExit *x;
    if (exit_idx < 0) {
        return;
    }
    x = &w->warp_exits[exit_idx];
    r01_game_warp_to_tile(ctx, x->dest_screen_col, x->dest_screen_row, x->dest_tile_col,
                          x->dest_tile_row);
}

void r01_game_warp_check(R01GameCtx *ctx, const R01World *w) {
    int hx, hy, hw, hh;
    int hit;
    if (!ctx || !w || r01_game_fade_active(ctx)) {
        return;
    }
    r01_play_player_hit_rect(w, ctx, ctx->player_x, ctx->player_y, &hx, &hy, &hw, &hh);
    hit = r01_world_warp_entrance_hit(w, hx, hy, hw, hh);
    if (hit < 0) {
        return;
    }
    {
        int exit_idx = r01_world_warp_exit_for_entrance(w, hit);
        if (exit_idx < 0) {
            return;
        }
        if (w->warp_exits[exit_idx].flags & R01_WARP_FADE_OUT) {
            ctx->fade_pending_entrance = hit;
            r01_game_fade_start(ctx, (w->warp_exits[exit_idx].flags & R01_WARP_FADE_WHITE) != 0,
                                R01_FADE_MAX);
            return;
        }
        warp_execute_exit(ctx, w, hit);
        if (w->warp_exits[exit_idx].flags & R01_WARP_FADE_IN) {
            ctx->fade_pending_entrance = -1;
            r01_game_fade_start(ctx, (w->warp_exits[exit_idx].flags & R01_WARP_FADE_WHITE) != 0, 0);
        }
    }
}

void r01_game_fade_warp_step(R01GameCtx *ctx, const R01World *w) {
    int exit_idx;
    if (!ctx || !w || ctx->fade_pending_entrance < 0) {
        return;
    }
    if (ctx->fade_level != R01_FADE_MAX) {
        return;
    }
    warp_execute_exit(ctx, w, ctx->fade_pending_entrance);
    exit_idx = r01_world_warp_exit_for_entrance(w, ctx->fade_pending_entrance);
    ctx->fade_pending_entrance = -1;
    if (exit_idx >= 0 && (w->warp_exits[exit_idx].flags & R01_WARP_FADE_IN)) {
        r01_game_fade_start(ctx, (w->warp_exits[exit_idx].flags & R01_WARP_FADE_WHITE) != 0, 0);
    } else {
        ctx->fade_target = 0;
    }
}

int r01_game_warp_by_id(R01GameCtx *ctx, const R01World *w, const char *warp_id) {
    int i;
    if (!ctx || !w || !warp_id) {
        return 0;
    }
    for (i = 0; i < w->warp_entrance_count; i++) {
        if (!w->warp_entrances[i].present) {
            continue;
        }
        if (strcmp(w->warp_entrances[i].id, warp_id) == 0) {
            warp_execute_exit(ctx, w, i);
            return 1;
        }
    }
    return 0;
}

int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed) {
    int i;
    int mag;
    int slot = -1;
    if (!ctx) {
        return -1;
    }
    if (dx == 0 && dy == 0) {
        return -1;
    }
    if (speed < 1) {
        speed = R01_PROJ_SPEED_DEFAULT;
    }
    mag = (int)(sqrt((double)(dx * dx + dy * dy)) + 0.5);
    if (mag < 1) {
        return -1;
    }
    for (i = 0; i < R01_MAX_PROJECTILES; i++) {
        if (!ctx->projectiles[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }
    ctx->projectiles[slot].active = 1;
    ctx->projectiles[slot].x = (ctx->player_x + R01_PLAY_PLAYER_W / 2) << R01_PROJ_FIXED_SHIFT;
    ctx->projectiles[slot].y = (ctx->player_y + R01_PLAY_PLAYER_H / 2) << R01_PROJ_FIXED_SHIFT;
    ctx->projectiles[slot].vx = (dx * speed * (1 << R01_PROJ_FIXED_SHIFT)) / mag;
    ctx->projectiles[slot].vy = (dy * speed * (1 << R01_PROJ_FIXED_SHIFT)) / mag;
    ctx->projectiles[slot].ttl = R01_PROJECTILE_TTL;
    ctx->projectiles[slot].tile = 2;
    ctx->projectiles[slot].pal = 0;
    return slot;
}

static int proj_solid_at(const R01World *w, int wx, int wy) {
    int sc = wx / R01_SCREEN_PX_W;
    int sr = wy / R01_SCREEN_PX_H;
    int lx = wx - sc * R01_SCREEN_PX_W;
    int ly = wy - sr * R01_SCREEN_PX_H;
    int cell;
    const R01Screen *s;
    int si;
    if (!w) {
        return 0;
    }
    for (si = 0; si < w->screen_count; si++) {
        s = &w->screens[si];
        if (!s->present || s->col != sc || s->row != sr) {
            continue;
        }
        cell = (ly / 8) * R01_SCREEN_TILES_X + (lx / 8);
        if (cell >= 0 && cell < R01_TILES_PER_SCREEN) {
            return (s->attrs[cell] & R01_ATTR_SOLID) != 0;
        }
        return 0;
    }
    return 0;
}

void r01_projectile_tick(R01GameCtx *ctx, const R01World *w) {
    int i;
    if (!ctx) {
        return;
    }
    for (i = 0; i < R01_MAX_PROJECTILES; i++) {
        R01Projectile *p = &ctx->projectiles[i];
        int wx, wy;
        if (!p->active) {
            continue;
        }
        p->x += p->vx;
        p->y += p->vy;
        p->ttl--;
        wx = p->x >> R01_PROJ_FIXED_SHIFT;
        wy = p->y >> R01_PROJ_FIXED_SHIFT;
        if (p->ttl <= 0 || wx < 0 || wy < 0 || proj_solid_at(w, wx, wy)) {
            p->active = 0;
        }
    }
}

int r01_projectile_count_active(const R01GameCtx *ctx) {
    int i, n = 0;
    if (!ctx) {
        return 0;
    }
    for (i = 0; i < R01_MAX_PROJECTILES; i++) {
        if (ctx->projectiles[i].active) {
            n++;
        }
    }
    return n;
}

uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn) {
    if (!ctx) {
        return 0;
    }
    return (uint8_t)((ctx->pad >> btn) & 1u);
}

uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn) {
    if (!ctx) {
        return 0;
    }
    return (uint8_t)(((ctx->pad ^ ctx->pad_prev) & ctx->pad) >> btn) & 1u;
}

void r01_player_warp(R01GameCtx *ctx, int col, int row) {
    if (!ctx) {
        return;
    }
    ctx->player_x = R01_PLAY_SPAWN_CENTER_X(col);
    ctx->player_y = R01_PLAY_SPAWN_CENTER_Y(row);
    r01_game_camera_update(ctx);
}

void r01_player_set_type(uint8_t type_id) {
    (void)type_id;
}

void r01_camera_set_deadzone(R01GameCtx *ctx, int dx, int dy) {
    if (!ctx) {
        return;
    }
    if (dx < 0) {
        dx = 0;
    }
    if (dy < 0) {
        dy = 0;
    }
    ctx->cam_deadzone_x = dx;
    ctx->cam_deadzone_y = dy;
}

void r01_camera_set_axis_lock(R01GameCtx *ctx, int mode) {
    if (!ctx) {
        return;
    }
    if (mode < R01_CAM_AXIS_BOTH || mode > R01_CAM_AXIS_V) {
        mode = R01_CAM_AXIS_BOTH;
    }
    ctx->cam_axis_lock = mode;
}

int r01_event_on_button(uint8_t btn, R01EventFn fn) {
    int i;
    if (!fn) {
        return -1;
    }
    for (i = 0; i < R01_EVENT_SLOTS; i++) {
        if (!s_events[i].fn) {
            s_events[i].btn = btn;
            s_events[i].fn = fn;
            return i;
        }
    }
    return -1;
}

void r01_runtime_dispatch_buttons(R01GameCtx *ctx) {
    int i;
    if (!ctx) {
        return;
    }
    for (i = 0; i < R01_EVENT_SLOTS; i++) {
        if (s_events[i].fn && r01_pad_just_pressed(ctx, s_events[i].btn)) {
            s_events[i].fn(ctx);
        }
    }
}
