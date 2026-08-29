/* Host export runtime for custom_logic (copied to output/C/r01_runtime.c). */
#include "include/r01_engine.h"

#include <math.h>
#include <string.h>

#define R01_CAM_DEADZONE_X_DEFAULT 32
#define R01_CAM_DEADZONE_Y_DEFAULT 30
#define R01_CAM_AXIS_BOTH 0
#define R01_CAM_AXIS_H 1
#define R01_CAM_AXIS_V 2

static void play_camera_update(int *cam_x, int *cam_y, int player_x, int player_y, int player_w, int player_h,
                               int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock) {
    int ax, ay, target_x, target_y;
    if (!cam_x || !cam_y) {
        return;
    }
    ax = player_x + player_w / 2;
    ay = player_y + player_h / 2;
    target_x = ax - screen_w / 2;
    target_y = ay - screen_h / 2;
    if (axis_lock != R01_CAM_AXIS_V) {
        if (deadzone_x > 0) {
            if (ax - *cam_x < deadzone_x) {
                *cam_x = ax - deadzone_x;
            } else if (ax - *cam_x > screen_w - deadzone_x - player_w) {
                *cam_x = ax - (screen_w - deadzone_x - player_w);
            }
        } else {
            *cam_x = target_x;
        }
    }
    if (axis_lock != R01_CAM_AXIS_H) {
        if (deadzone_y > 0) {
            if (ay - *cam_y < deadzone_y) {
                *cam_y = ay - deadzone_y;
            } else if (ay - *cam_y > screen_h - deadzone_y - player_h) {
                *cam_y = ay - (screen_h - deadzone_y - player_h);
            }
        } else {
            *cam_y = target_y;
        }
    }
    if (*cam_x < 0) {
        *cam_x = 0;
    }
    if (*cam_y < 0) {
        *cam_y = 0;
    }
}

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
extern const int player_state_frames[4];

static struct {
    uint8_t btn;
    R01EventFn fn;
} s_events[R01_EVENT_SLOTS];

void r01_game_ctx_init(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->cam_deadzone_x = R01_CAM_DEADZONE_X_DEFAULT;
    ctx->cam_deadzone_y = R01_CAM_DEADZONE_Y_DEFAULT;
    ctx->cam_axis_lock = R01_CAM_AXIS_BOTH;
    ctx->fade_color = R01_FADE_BLACK;
    ctx->fade_pending_entrance = -1;
    r01_player_anim_init(ctx);
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
    if (!ctx) {
        return;
    }
    play_camera_update(&ctx->cam_x, &ctx->cam_y, ctx->player_x, ctx->player_y, R01_PLAY_PLAYER_W,
                           R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H, ctx->cam_deadzone_x,
                           ctx->cam_deadzone_y, ctx->cam_axis_lock);
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

static int pa_dir_from_delta(int dx, int dy) {
    if (dx > 0 && dy == 0) {
        return R01_PLAYER_DIR_RIGHT;
    }
    if (dx > 0 && dy > 0) {
        return R01_PLAYER_DIR_DOWN_RIGHT;
    }
    if (dx == 0 && dy > 0) {
        return R01_PLAYER_DIR_DOWN;
    }
    if (dx < 0 && dy > 0) {
        return R01_PLAYER_DIR_DOWN_LEFT;
    }
    if (dx < 0 && dy == 0) {
        return R01_PLAYER_DIR_LEFT;
    }
    if (dx < 0 && dy < 0) {
        return R01_PLAYER_DIR_UP_LEFT;
    }
    if (dx == 0 && dy < 0) {
        return R01_PLAYER_DIR_UP;
    }
    if (dx > 0 && dy < 0) {
        return R01_PLAYER_DIR_UP_RIGHT;
    }
    return -1;
}

static int pa_face_to_dir(int face) {
    switch (face) {
    case R01_PLAYER_FACE_DOWN:
        return R01_PLAYER_DIR_DOWN;
    case R01_PLAYER_FACE_LEFT:
        return R01_PLAYER_DIR_LEFT;
    case R01_PLAYER_FACE_UP:
        return R01_PLAYER_DIR_UP;
    default:
        return R01_PLAYER_DIR_RIGHT;
    }
}

static int pa_dir_flip_h(int dir) {
    return dir == R01_PLAYER_DIR_LEFT || dir == R01_PLAYER_DIR_UP_LEFT || dir == R01_PLAYER_DIR_DOWN_LEFT;
}

static void pa_apply_idle_facing(R01GameCtx *ctx) {
    ctx->player_anim_dir = pa_face_to_dir(ctx->player_default_face);
    ctx->player_anim_flip_h = pa_dir_flip_h(ctx->player_anim_dir);
}

void r01_player_anim_init(R01GameCtx *ctx) {
    int i;
    if (!ctx) {
        return;
    }
    ctx->player_anim_state = 0;
    ctx->player_anim_frame = 0;
    ctx->player_anim_ctr = 0;
    ctx->player_anim_flip_h = 0;
    ctx->player_anim_dir = R01_PLAYER_DIR_RIGHT;
    ctx->player_anim_moving = 0;
    ctx->player_default_face = R01_PLAYER_FACE_RIGHT;
    ctx->player_idle_state = 0;
    for (i = 0; i < 8; i++) {
        ctx->player_walk_state[i] = 1;
    }
    for (i = 0; i < 4; i++) {
        ctx->player_state_delay[i] = 6;
    }
    pa_apply_idle_facing(ctx);
}

void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= 4) {
        return;
    }
    ctx->player_idle_state = entity_state_idx;
    if (!ctx->player_anim_moving) {
        ctx->player_anim_state = entity_state_idx;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
    }
}

void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx) {
    if (!ctx || dir8 < 0 || dir8 > 7 || entity_state_idx < 0 || entity_state_idx >= 4) {
        return;
    }
    ctx->player_walk_state[dir8] = entity_state_idx;
    if (ctx->player_anim_moving && ctx->player_anim_dir == dir8) {
        ctx->player_anim_state = entity_state_idx;
    }
}

void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx) {
    int i;
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= 4) {
        return;
    }
    for (i = 0; i < 8; i++) {
        ctx->player_walk_state[i] = entity_state_idx;
    }
    if (ctx->player_anim_moving) {
        ctx->player_anim_state = entity_state_idx;
    }
}

void r01_player_default_face_set(R01GameCtx *ctx, int face) {
    if (!ctx) {
        return;
    }
    if (face < R01_PLAYER_FACE_RIGHT || face > R01_PLAYER_FACE_UP) {
        face = R01_PLAYER_FACE_RIGHT;
    }
    ctx->player_default_face = face;
    if (!ctx->player_anim_moving) {
        pa_apply_idle_facing(ctx);
    }
}

void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= 4) {
        return;
    }
    if (ticks < 1) {
        ticks = 1;
    }
    ctx->player_state_delay[entity_state_idx] = ticks;
}

void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy) {
    int new_dir;
    int prev_state;
    if (!ctx) {
        return;
    }
    if (dx != 0 || dy != 0) {
        new_dir = pa_dir_from_delta(dx, dy);
        if (new_dir >= 0) {
            ctx->player_anim_dir = new_dir;
        }
        ctx->player_anim_moving = 1;
        ctx->player_anim_flip_h = pa_dir_flip_h(ctx->player_anim_dir);
        prev_state = ctx->player_anim_state;
        ctx->player_anim_state = ctx->player_walk_state[ctx->player_anim_dir];
        if (ctx->player_anim_state != prev_state) {
            ctx->player_anim_frame = 0;
            ctx->player_anim_ctr = 0;
        }
        return;
    }
    if (ctx->player_anim_moving) {
        ctx->player_anim_moving = 0;
        ctx->player_anim_state = ctx->player_idle_state;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
    }
}

void r01_player_anim_tick(R01GameCtx *ctx) {
    int delay;
    int frame_count;
    if (!ctx) {
        return;
    }
    if (ctx->player_anim_state < 0 || ctx->player_anim_state >= 4) {
        return;
    }
    frame_count = player_state_frames[ctx->player_anim_state];
    if (frame_count < 1) {
        return;
    }
    if (frame_count <= 1) {
        ctx->player_anim_frame = 0;
        return;
    }
    delay = ctx->player_state_delay[ctx->player_anim_state];
    if (delay < 1) {
        delay = 1;
    }
    ctx->player_anim_ctr++;
    if (ctx->player_anim_ctr < delay) {
        return;
    }
    ctx->player_anim_ctr = 0;
    ctx->player_anim_frame++;
    if (ctx->player_anim_frame >= frame_count) {
        ctx->player_anim_frame = 0;
    }
}

int r01_player_anim_entity_state(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_state : 0;
}

int r01_player_anim_frame(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_frame : 0;
}

int r01_player_anim_flip_h(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_flip_h : 0;
}

int r01_player_anim_moving(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_moving : 0;
}
