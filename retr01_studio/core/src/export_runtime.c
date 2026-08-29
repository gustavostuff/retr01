/* Host export runtime for custom_logic (copied to output/C/r01_runtime.c). */
#include "include/r01_engine.h"

#include <math.h>
#include <string.h>

#define R01_EVENT_SLOTS 4
#define R01_MAX_PROJECTILES 8
#define R01_PROJECTILE_TTL 180
#define R01_FADE_SPEED 8
#define R01_PROJ_SPEED_DEFAULT 3
#define R01_PROJ_FIXED_SHIFT 8
#define R01_PLAY_PLAYER_W 8
#define R01_PLAY_PLAYER_H 8
#define R01_SCREEN_PX_W 128
#define R01_SCREEN_PX_H 120
#define R01_WARP_FADE_OUT 0x01u
#define R01_WARP_FADE_IN 0x02u
#define R01_WARP_FADE_WHITE 0x04u

typedef struct {
    const char *id;
    int sc, sr, tc, tr;
} R01WarpEntRec;
typedef struct {
    int ent;
    int dsc, dsr, dtc, dtr;
    uint8_t flags;
} R01WarpExitRec;

extern const R01WarpEntRec warp_ents[];
extern const int warp_ent_count;
extern const R01WarpExitRec warp_exits[];
extern const int warp_exit_count;

static struct {
    uint8_t btn;
    R01EventFn fn;
} s_events[R01_EVENT_SLOTS];

void r01_game_ctx_init(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->cam_axis_lock = R01_CAM_AXIS_BOTH;
    ctx->fade_color = R01_FADE_BLACK;
    ctx->fade_pending_entrance = -1;
}

static void warp_tile_world_pos(int screen_col, int screen_row, int tile_col, int tile_row, int *out_wx,
                                int *out_wy) {
    if (out_wx) {
        *out_wx = screen_col * R01_SCREEN_PX_W + tile_col * 8;
    }
    if (out_wy) {
        *out_wy = screen_row * R01_SCREEN_PX_H + tile_row * 8;
    }
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
        if (ctx->cam_deadzone_x <= 0) {
            ctx->cam_x = target_x;
        }
    }
    if (ctx->cam_axis_lock != R01_CAM_AXIS_H) {
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

void r01_game_warp_to_tile(R01GameCtx *ctx, int screen_col, int screen_row, int tile_col, int tile_row) {
    if (!ctx) {
        return;
    }
    warp_tile_world_pos(screen_col, screen_row, tile_col, tile_row, &ctx->player_x, &ctx->player_y);
    r01_game_camera_update(ctx);
}

static void warp_execute_exit(R01GameCtx *ctx, int entrance_idx) {
    int i;
    if (!ctx || entrance_idx < 0) {
        return;
    }
    for (i = 0; i < warp_exit_count; i++) {
        const R01WarpExitRec *x = &warp_exits[i];
        if (x->ent != entrance_idx) {
            continue;
        }
        r01_game_warp_to_tile(ctx, x->dsc, x->dsr, x->dtc, x->dtr);
        return;
    }
}

int r01_game_warp_by_id(R01GameCtx *ctx, const char *warp_id) {
    int i;
    if (!ctx || !warp_id) {
        return 0;
    }
    for (i = 0; i < warp_ent_count; i++) {
        if (warp_ents[i].id && strcmp(warp_ents[i].id, warp_id) == 0) {
            warp_execute_exit(ctx, i);
            return 1;
        }
    }
    return 0;
}

void r01_game_warp_check(R01GameCtx *ctx) {
    int i;
    if (!ctx || r01_game_fade_active(ctx)) {
        return;
    }
    for (i = 0; i < warp_ent_count; i++) {
        const R01WarpEntRec *e = &warp_ents[i];
        int tx, ty;
        warp_tile_world_pos(e->sc, e->sr, e->tc, e->tr, &tx, &ty);
        if (ctx->player_x < tx + 8 && ctx->player_x + R01_PLAY_PLAYER_W > tx && ctx->player_y < ty + 8 &&
            ctx->player_y + R01_PLAY_PLAYER_H > ty) {
            warp_execute_exit(ctx, i);
            return;
        }
    }
}

int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed) {
    int i, mag, slot = -1;
    if (!ctx || (dx == 0 && dy == 0)) {
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

void r01_projectile_tick(R01GameCtx *ctx) {
    int i;
    if (!ctx) {
        return;
    }
    for (i = 0; i < R01_MAX_PROJECTILES; i++) {
        if (!ctx->projectiles[i].active) {
            continue;
        }
        ctx->projectiles[i].x += ctx->projectiles[i].vx;
        ctx->projectiles[i].y += ctx->projectiles[i].vy;
        ctx->projectiles[i].ttl--;
        if (ctx->projectiles[i].ttl <= 0) {
            ctx->projectiles[i].active = 0;
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
    ctx->player_x = col * R01_SCREEN_PX_W + (R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2;
    ctx->player_y = row * R01_SCREEN_PX_H + (R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2;
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

int r01_entity_spawn(uint8_t type, int wx, int wy) {
    (void)type;
    (void)wx;
    (void)wy;
    return -1;
}

void r01_entity_remove(int inst) {
    (void)inst;
}

void r01_world_warp_screen(int col, int row) {
    (void)col;
    (void)row;
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
