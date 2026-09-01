#include "r01_play_anim.h"

#include <string.h>

static int dir_from_delta(int dx, int dy) {
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

static int face_to_dir(int face) {
    switch (face) {
    case R01_PLAYER_FACE_DOWN:
        return R01_PLAYER_DIR_DOWN;
    case R01_PLAYER_FACE_LEFT:
        return R01_PLAYER_DIR_LEFT;
    case R01_PLAYER_FACE_UP:
        return R01_PLAYER_DIR_UP;
    case R01_PLAYER_FACE_RIGHT:
    default:
        return R01_PLAYER_DIR_RIGHT;
    }
}

static int dir_flip_h(int dir) {
    return dir == R01_PLAYER_DIR_LEFT || dir == R01_PLAYER_DIR_UP_LEFT || dir == R01_PLAYER_DIR_DOWN_LEFT;
}

static void apply_idle_facing(R01PlayAnimCtx *ctx) {
    if (!ctx) {
        return;
    }
    ctx->player_anim_dir = face_to_dir(ctx->player_default_face);
    ctx->player_anim_flip_h = dir_flip_h(ctx->player_anim_dir);
}

void r01_play_anim_init(R01PlayAnimCtx *ctx) {
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
    for (i = 0; i < R01_PLAY_ANIM_STATES_MAX; i++) {
        ctx->player_state_delay[i] = R01_PLAY_ANIM_DELAY_DEFAULT;
    }
    apply_idle_facing(ctx);
}

void r01_play_anim_set_idle_state(R01PlayAnimCtx *ctx, int entity_state_idx) {
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    ctx->player_idle_state = entity_state_idx;
    if (!ctx->player_anim_moving) {
        ctx->player_anim_state = entity_state_idx;
        ctx->player_anim_frame = 0;
        ctx->player_anim_ctr = 0;
    }
}

void r01_play_anim_set_walk_state(R01PlayAnimCtx *ctx, int dir8, int entity_state_idx) {
    if (!ctx || dir8 < 0 || dir8 > 7 || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    ctx->player_walk_state[dir8] = entity_state_idx;
    if (ctx->player_anim_moving && ctx->player_anim_dir == dir8) {
        ctx->player_anim_state = entity_state_idx;
    }
}

void r01_play_anim_set_walk_all(R01PlayAnimCtx *ctx, int entity_state_idx) {
    int i;
    if (!ctx || entity_state_idx < 0 || entity_state_idx >= R01_PLAY_ANIM_STATES_MAX) {
        return;
    }
    for (i = 0; i < 8; i++) {
        ctx->player_walk_state[i] = entity_state_idx;
    }
    if (ctx->player_anim_moving) {
        ctx->player_anim_state = entity_state_idx;
    }
}

void r01_play_default_face_set(R01PlayAnimCtx *ctx, int face) {
    if (!ctx) {
        return;
    }
    if (face < R01_PLAYER_FACE_RIGHT || face > R01_PLAYER_FACE_UP) {
        face = R01_PLAYER_FACE_RIGHT;
    }
    ctx->player_default_face = face;
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
    ctx->player_state_delay[entity_state_idx] = ticks;
}

void r01_play_anim_update(R01PlayAnimCtx *ctx, int dx, int dy) {
    int new_dir;
    int prev_state;
    if (!ctx) {
        return;
    }
    if (dx != 0 || dy != 0) {
        new_dir = dir_from_delta(dx, dy);
        if (new_dir >= 0) {
            ctx->player_anim_dir = new_dir;
        }
        ctx->player_anim_moving = 1;
        ctx->player_anim_flip_h = dir_flip_h(ctx->player_anim_dir);
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

int r01_play_anim_entity_state(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_state : 0;
}

int r01_play_anim_frame(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_frame : 0;
}

int r01_play_anim_flip_h(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_flip_h : 0;
}

int r01_play_anim_dir(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_dir : R01_PLAYER_DIR_RIGHT;
}

int r01_play_anim_moving(const R01PlayAnimCtx *ctx) {
    return ctx ? ctx->player_anim_moving : 0;
}
