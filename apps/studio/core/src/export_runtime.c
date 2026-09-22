/* Host export runtime for custom_logic (copied to output/C/r01_runtime.c).
 * Links apps/common r01_play_camera.c / r01_play_anim.c (Studio compile_custom_plugin). */
#include "include/r01_engine.h"
#include "r01_play_anim.h"
#include "r01_play_camera.h"
#include "r01_cart_caps.h"

#include <math.h>
#include <string.h>

#define R01_EVENT_SLOTS 4
#define R01_MAX_PROJECTILES 8
#define R01_PROJECTILE_TTL 180
#define R01_FADE_SPEED 8
#define R01_PROJ_FIXED_SHIFT 8
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
    ctx->game_mode = R01_GAME_MODE_TOPDOWN;
    ctx->plat_gravity = R01_PLAT_GRAVITY_DEFAULT;
    ctx->plat_jump = R01_PLAT_JUMP_DEFAULT;
    ctx->plat_meter = R01_PLAT_METER_DEFAULT;
    ctx->fade_color = R01_FADE_BLACK;
    ctx->fade_pending_entrance = -1;
    ctx->player_move_mul = 1;
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
    r01_play_camera_update(&ctx->cam_x, &ctx->cam_y, ctx->player_x, ctx->player_y, R01_PLAY_PLAYER_W,
                           R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H, ctx->cam_deadzone_x,
                           ctx->cam_deadzone_y, ctx->cam_axis_lock);
}

void r01_game_camera_snap(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    r01_play_camera_snap(&ctx->cam_x, &ctx->cam_y, ctx->player_x, ctx->player_y, R01_PLAY_PLAYER_W,
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

int r01_pad_down(const R01GameCtx *ctx, uint8_t mask) {
    if (!ctx || mask == 0) {
        return 0;
    }
    return (ctx->pad & mask) != 0;
}

void r01_player_warp(R01GameCtx *ctx, int col, int row) {
    if (!ctx) {
        return;
    }
    ctx->player_x = col * R01_SCREEN_PX_W + (R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2;
    ctx->player_y = row * R01_SCREEN_PX_H + (R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2;
    ctx->plat_vel_y = 0;
    ctx->plat_frac_x = 0;
    ctx->plat_frac_y = 0;
    ctx->plat_grounded = 0;
    ctx->plat_jump_held = 0;
    r01_game_camera_snap(ctx);
}

void r01_player_set_type(uint8_t type_id) {
    (void)type_id;
}

int r01_player_moving_x(const R01GameCtx *ctx) {
    return r01_pad_down(ctx, (uint8_t)(R01_PAD_LEFT | R01_PAD_RIGHT));
}

void r01_player_set_move_mul(R01GameCtx *ctx, int mul) {
    if (!ctx) {
        return;
    }
    if (mul < 1) {
        mul = 1;
    }
    if (mul > 8) {
        mul = 8;
    }
    ctx->player_move_mul = mul;
}

int r01_player_move_mul(const R01GameCtx *ctx) {
    int mul;
    if (!ctx) {
        return 1;
    }
    mul = ctx->player_move_mul;
    if (mul < 1) {
        return 1;
    }
    if (mul > 8) {
        return 8;
    }
    return mul;
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

void r01_camera_disable_deadzone(R01GameCtx *ctx) {
    r01_camera_set_deadzone(ctx, R01_CAM_DEADZONE_OFF, R01_CAM_DEADZONE_OFF);
}

void r01_bg0_set_wrap(R01GameCtx *ctx, int wrap_x, int wrap_y) {
    if (!ctx) {
        return;
    }
    ctx->bg0_wrap_x = wrap_x ? 1 : 0;
    ctx->bg0_wrap_y = wrap_y ? 1 : 0;
}

void r01_bg0_set_clip_to_bg1(R01GameCtx *ctx, int enable) {
    if (!ctx) {
        return;
    }
    ctx->bg0_clip_bg1 = enable ? 1 : 0;
}

void r01_bgm_play(R01GameCtx *ctx, int track) {
    if (!ctx) {
        return;
    }
    if (track < 1) {
        track = 1;
    }
    ctx->bgm_track = track;
}

void r01_bgm_stop(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    ctx->bgm_track = 0;
}

void r01_sfx_play(R01GameCtx *ctx, int id) {
    (void)ctx;
    (void)id;
    /* Host Play / emu: tracker SFX on voices 6-8. */
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

void r01_game_set_mode(R01GameCtx *ctx, int mode) {
    if (!ctx) {
        return;
    }
    if (mode != R01_GAME_MODE_PLATFORMER) {
        mode = R01_GAME_MODE_TOPDOWN;
    }
    ctx->game_mode = mode;
    if (mode == R01_GAME_MODE_TOPDOWN) {
        ctx->plat_vel_y = 0;
        ctx->plat_frac_x = 0;
        ctx->plat_frac_y = 0;
        ctx->plat_grounded = 0;
        ctx->plat_jump_held = 0;
    }
}

void r01_solid_pattern_add(R01GameCtx *ctx, int bank, int tile) {
    int i;
    if (!ctx) {
        return;
    }
    if (bank < 0 || bank > 15 || tile < 0 || tile > 255) {
        return;
    }
    for (i = 0; i < (int)ctx->solid_pat_count && i < 64; i++) {
        if ((int)ctx->solid_pat_bank[i] == bank && (int)ctx->solid_pat_tile[i] == tile) {
            return;
        }
    }
    if (ctx->solid_pat_count >= 64) {
        return;
    }
    ctx->solid_pat_bank[ctx->solid_pat_count] = (uint8_t)bank;
    ctx->solid_pat_tile[ctx->solid_pat_count] = (uint8_t)tile;
    ctx->solid_pat_count++;
}

void r01_platformer_set_gravity(R01GameCtx *ctx, int units) {
    if (!ctx) {
        return;
    }
    if (units < 1) {
        units = R01_PLAT_GRAVITY_DEFAULT;
    }
    if (units > R01_PLAT_GRAVITY_MAX) {
        units = R01_PLAT_GRAVITY_MAX;
    }
    ctx->plat_gravity = units;
}

void r01_platformer_set_jump(R01GameCtx *ctx, int impulse) {
    if (!ctx) {
        return;
    }
    if (impulse < 1) {
        impulse = R01_PLAT_JUMP_DEFAULT;
    }
    if (impulse > 32) {
        impulse = 32;
    }
    ctx->plat_jump = impulse;
}

void r01_platformer_set_meter(R01GameCtx *ctx, int px_per_meter) {
    if (!ctx) {
        return;
    }
    if (px_per_meter < 1) {
        px_per_meter = R01_PLAT_METER_DEFAULT;
    }
    if (px_per_meter > 64) {
        px_per_meter = 64;
    }
    ctx->plat_meter = px_per_meter;
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

void r01_entity_set_active(int inst, int on) {
    (void)inst;
    (void)on;
}

void r01_entity_set_xy(int inst, int wx, int wy) {
    (void)inst;
    (void)wx;
    (void)wy;
}

void r01_entity_get_xy(int inst, int *wx, int *wy) {
    if (wx) {
        *wx = 0;
    }
    if (wy) {
        *wy = 0;
    }
    (void)inst;
}

void r01_entity_set_state(int inst, uint8_t state) {
    (void)inst;
    (void)state;
}

void r01_entity_set_frame(int inst, uint8_t frame) {
    (void)inst;
    (void)frame;
}

void r01_entity_set_flip(int inst, int flip_h, int flip_v) {
    (void)inst;
    (void)flip_h;
    (void)flip_v;
}

void r01_entity_get_flip(int inst, int *flip_h, int *flip_v) {
    if (flip_h) {
        *flip_h = 0;
    }
    if (flip_v) {
        *flip_v = 0;
    }
    (void)inst;
}

void r01_entity_set_rot90(int inst, uint8_t quad) {
    (void)inst;
    (void)quad;
}

int r01_entity_alive(int inst) {
    (void)inst;
    return 0;
}

int r01_entity_count_live(void) {
    return 0;
}

uint8_t r01_entity_type(int inst) {
    (void)inst;
    return 0;
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

#define R01_PLAYER_ANIM_WRAP(call)                                                                 \
    do {                                                                                           \
        R01PlayAnimCtx _a;                                                                         \
        if (!ctx) {                                                                                \
            return;                                                                                \
        }                                                                                          \
        R01_PLAY_ANIM_PULL(&_a, ctx);                                                              \
        call;                                                                                      \
        R01_PLAY_ANIM_PUSH(&_a, ctx);                                                              \
    } while (0)

void r01_player_anim_init(R01GameCtx *ctx) {
    R01PlayAnimCtx a;
    if (!ctx) {
        return;
    }
    r01_play_anim_init(&a);
    R01_PLAY_ANIM_PUSH(&a, ctx);
}

void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_idle_state(&_a, entity_state_idx));
}

void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_walk_state(&_a, dir8, entity_state_idx));
}

void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_walk_all(&_a, entity_state_idx));
}

void r01_player_anim_set_crouch_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_crouch_state(&_a, entity_state_idx));
}

void r01_player_anim_set_jump_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_jump_state(&_a, entity_state_idx));
}

void r01_player_anim_set_frame_delay(R01GameCtx *ctx, int ticks) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_frame_delay(&_a, ticks));
}

int r01_player_anim_frame_delay(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_delay_override : 0;
}

void r01_player_default_face_set(R01GameCtx *ctx, int face) {
    R01_PLAYER_ANIM_WRAP(r01_play_default_face_set(&_a, face));
}

void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks) {
    R01_PLAYER_ANIM_WRAP(r01_play_state_frame_delay_set(&_a, entity_state_idx, ticks));
}

void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_update(&_a, dx, dy));
}

void r01_player_anim_tick(R01GameCtx *ctx) {
    int delay;
    int frame_count;
    if (!ctx) {
        return;
    }
    if (ctx->player_idle_state < 0 && ctx->player_anim_state == 0) {
        ctx->player_anim_frame = 0;
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
    if (ctx->player_anim_delay_override > 0) {
        delay = ctx->player_anim_delay_override;
        if (delay < 1) {
            delay = 1;
        }
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
