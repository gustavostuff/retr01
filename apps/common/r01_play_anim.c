#include "r01_play_anim.h"

#include <string.h>

static int8_t dir_from_delta(int8_t dx, int8_t dy) {
    if (dx > 0 && dy == 0) {
        return (int8_t)R01_PLAYER_DIR_RIGHT;
    }
    if (dx > 0 && dy > 0) {
        return (int8_t)R01_PLAYER_DIR_DOWN_RIGHT;
    }
    if (dx == 0 && dy > 0) {
        return (int8_t)R01_PLAYER_DIR_DOWN;
    }
    if (dx < 0 && dy > 0) {
        return (int8_t)R01_PLAYER_DIR_DOWN_LEFT;
    }
    if (dx < 0 && dy == 0) {
        return (int8_t)R01_PLAYER_DIR_LEFT;
    }
    if (dx < 0 && dy < 0) {
        return (int8_t)R01_PLAYER_DIR_UP_LEFT;
    }
    if (dx == 0 && dy < 0) {
        return (int8_t)R01_PLAYER_DIR_UP;
    }
    if (dx > 0 && dy < 0) {
        return (int8_t)R01_PLAYER_DIR_UP_RIGHT;
    }
    return -1;
}

static uint8_t face_to_dir(uint8_t face) {
    switch (face) {
    case R01_PLAYER_FACE_DOWN:
        return (uint8_t)R01_PLAYER_DIR_DOWN;
    case R01_PLAYER_FACE_LEFT:
        return (uint8_t)R01_PLAYER_DIR_LEFT;
    case R01_PLAYER_FACE_UP:
        return (uint8_t)R01_PLAYER_DIR_UP;
    case R01_PLAYER_FACE_RIGHT:
    default:
        return (uint8_t)R01_PLAYER_DIR_RIGHT;
    }
}

static uint8_t dir_flip_h(uint8_t dir) {
    return (uint8_t)(dir == R01_PLAYER_DIR_LEFT || dir == R01_PLAYER_DIR_UP_LEFT ||
                     dir == R01_PLAYER_DIR_DOWN_LEFT);
}

static void apply_idle_facing(R01PlayAnimCtx *ctx) {
    if (!ctx) {
        return;
    }
    ctx->player_anim_dir = face_to_dir(ctx->player_default_face);
    ctx->player_anim_flip_h = dir_flip_h(ctx->player_anim_dir);
}

void r01_play_anim_init(R01PlayAnimCtx *ctx) {
    uint8_t i;
    if (!ctx) {
        return;
    }
    ctx->player_anim_state = 0;
    ctx->player_anim_frame = 0;
    ctx->player_anim_ctr = 0;
    ctx->player_anim_flip_h = 0;
    ctx->player_anim_dir = (uint8_t)R01_PLAYER_DIR_RIGHT;
    ctx->player_anim_moving = 0;
    ctx->player_default_face = (uint8_t)R01_PLAYER_FACE_RIGHT;
    ctx->player_idle_state = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
    ctx->player_crouch_state = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
    ctx->player_crouching = 0;
    ctx->player_jump_state = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
    ctx->player_airborne = 0;
    ctx->player_anim_delay_override = 0;
    for (i = 0; i < 8u; i++) {
        ctx->player_walk_state[i] = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
    }
    for (i = 0; i < (uint8_t)R01_PLAY_ANIM_STATES_MAX; i++) {
        ctx->player_state_delay[i] = (uint8_t)R01_PLAY_ANIM_DELAY_DEFAULT;
        /* Default matches Studio authoring: release d-pad -> idle. Opt out per state
         * with r01_play_anim_set_release_to_idle(ctx, state, 0) to hold pose (e.g. slides). */
        ctx->player_release_to_idle[i] = 1;
    }
    apply_idle_facing(ctx);
}

void r01_play_anim_set_idle_state(R01PlayAnimCtx *ctx, int entity_state_idx) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    ctx->player_idle_state = (uint8_t)entity_state_idx;
    if (!ctx->player_anim_moving) {
        ctx->player_anim_state = (uint8_t)entity_state_idx;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
    }
}

void r01_play_anim_set_walk_state(R01PlayAnimCtx *ctx, int dir8, int entity_state_idx) {
    if (!ctx || dir8 < 0 || dir8 > 7 || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    ctx->player_walk_state[dir8] = (uint8_t)entity_state_idx;
    if (ctx->player_anim_moving && ctx->player_anim_dir == (uint8_t)dir8) {
        ctx->player_anim_state = (uint8_t)entity_state_idx;
    }
}

void r01_play_anim_set_walk_all(R01PlayAnimCtx *ctx, int entity_state_idx) {
    uint8_t i;
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    for (i = 0; i < 8u; i++) {
        ctx->player_walk_state[i] = (uint8_t)entity_state_idx;
    }
    if (ctx->player_anim_moving) {
        ctx->player_anim_state = (uint8_t)entity_state_idx;
    }
}

void r01_play_anim_set_crouch_state(R01PlayAnimCtx *ctx, int entity_state_idx) {
    if (!ctx) {
        return;
    }
    if (entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        ctx->player_crouch_state = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
        return;
    }
    ctx->player_crouch_state = (uint8_t)entity_state_idx;
}

void r01_play_anim_set_jump_state(R01PlayAnimCtx *ctx, int entity_state_idx) {
    if (!ctx) {
        return;
    }
    if (entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        ctx->player_jump_state = (uint8_t)R01_PLAY_ANIM_UNMAPPED;
        return;
    }
    ctx->player_jump_state = (uint8_t)entity_state_idx;
}

void r01_play_anim_set_crouching(R01PlayAnimCtx *ctx, int on) {
    if (!ctx) {
        return;
    }
    ctx->player_crouching = on ? 1u : 0u;
}

void r01_play_anim_set_airborne(R01PlayAnimCtx *ctx, int on) {
    if (!ctx) {
        return;
    }
    ctx->player_airborne = on ? 1u : 0u;
}

void r01_play_anim_set_frame_delay(R01PlayAnimCtx *ctx, int ticks) {
    if (!ctx) {
        return;
    }
    if (ticks < 0) {
        ticks = 0;
    }
    if (ticks > 255) {
        ticks = 255;
    }
    ctx->player_anim_delay_override = (uint8_t)ticks;
}

uint8_t r01_play_anim_frame_delay(const R01PlayAnimCtx *ctx, uint8_t delay) {
    if (delay < 1u) {
        delay = 1u;
    }
    if (ctx && ctx->player_anim_delay_override > 0u) {
        delay = ctx->player_anim_delay_override;
        if (delay < 1u) {
            delay = 1u;
        }
    }
    return delay;
}

static void pa_show_mapped(R01PlayAnimCtx *ctx, uint8_t mapped) {
    if (!ctx) {
        return;
    }
    if (!R01_PLAY_ANIM_MAPPED(mapped)) {
        ctx->player_anim_state = 0;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
        return;
    }
    if (ctx->player_anim_state != mapped) {
        ctx->player_anim_state = mapped;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
    }
}

void r01_play_anim_set_release_to_idle(R01PlayAnimCtx *ctx, int entity_state_idx, int enable) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    ctx->player_release_to_idle[entity_state_idx] = enable ? 1u : 0u;
}

void r01_play_default_face_set(R01PlayAnimCtx *ctx, int face) {
    if (!ctx) {
        return;
    }
    if (face < R01_PLAYER_FACE_RIGHT || face > R01_PLAYER_FACE_UP) {
        face = R01_PLAYER_FACE_RIGHT;
    }
    ctx->player_default_face = (uint8_t)face;
    if (!ctx->player_anim_moving) {
        apply_idle_facing(ctx);
    }
}

void r01_play_state_frame_delay_set(R01PlayAnimCtx *ctx, int entity_state_idx, int ticks) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    if (ticks < 1) {
        ticks = 1;
    }
    if (ticks > 255) {
        ticks = 255;
    }
    ctx->player_state_delay[entity_state_idx] = (uint8_t)ticks;
}

void r01_play_anim_update(R01PlayAnimCtx *ctx, int8_t dx, int8_t dy) {
    int8_t new_dir;
    uint8_t pose;
    if (!ctx) {
        return;
    }
    if (dx != 0 || dy != 0) {
        new_dir = dir_from_delta(dx, dy);
        if (new_dir >= 0) {
            ctx->player_anim_dir = (uint8_t)new_dir;
            ctx->player_anim_flip_h = dir_flip_h(ctx->player_anim_dir);
        }
    }
    if (ctx->player_airborne && R01_PLAY_ANIM_MAPPED(ctx->player_jump_state)) {
        ctx->player_anim_moving = (uint8_t)((dx != 0) || (dy != 0));
        pa_show_mapped(ctx, ctx->player_jump_state);
        return;
    }
    if (ctx->player_crouching && R01_PLAY_ANIM_MAPPED(ctx->player_crouch_state)) {
        ctx->player_anim_moving = 0;
        pa_show_mapped(ctx, ctx->player_crouch_state);
        return;
    }
    if (dx != 0 || dy != 0) {
        ctx->player_anim_moving = 1;
        pose = ctx->player_walk_state[ctx->player_anim_dir];
        if (!R01_PLAY_ANIM_MAPPED(pose)) {
            pose = ctx->player_idle_state;
        }
        pa_show_mapped(ctx, pose);
        return;
    }
    if (ctx->player_anim_moving) {
        ctx->player_anim_moving = 0;
        if (!R01_PLAY_ANIM_MAPPED(ctx->player_idle_state) ||
            (R01_PLAY_ANIM_MAPPED(ctx->player_anim_state) &&
             ctx->player_release_to_idle[ctx->player_anim_state])) {
            pa_show_mapped(ctx, ctx->player_idle_state);
        }
        /* release_to_idle cleared: keep last movement state/tile/flip (e.g. slide hold). */
    } else if (!R01_PLAY_ANIM_MAPPED(ctx->player_idle_state)) {
        pa_show_mapped(ctx, (uint8_t)R01_PLAY_ANIM_UNMAPPED);
    } else if (R01_PLAY_ANIM_MAPPED(ctx->player_crouch_state) &&
               ctx->player_anim_state == ctx->player_crouch_state) {
        pa_show_mapped(ctx, ctx->player_idle_state);
    } else if (R01_PLAY_ANIM_MAPPED(ctx->player_jump_state) &&
               ctx->player_anim_state == ctx->player_jump_state) {
        pa_show_mapped(ctx, ctx->player_idle_state);
    }
}

uint8_t r01_play_anim_entity_state(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_state : 0;
}

uint8_t r01_play_anim_frame(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_frame : 0;
}

uint8_t r01_play_anim_flip_h(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_flip_h : 0;
}

uint8_t r01_play_anim_dir(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_dir : (uint8_t)R01_PLAYER_DIR_RIGHT;
}

uint8_t r01_play_anim_moving(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_moving : 0;
}
