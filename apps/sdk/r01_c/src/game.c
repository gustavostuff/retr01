#include "r01_engine.h"

void r01_game_ctx_init(R01GameCtx *ctx) {
    uint8_t *p;
    uint16_t n;
    uint8_t i;
    if (!ctx) {
        return;
    }
    p = (uint8_t *)ctx;
    for (n = 0; n < (uint16_t)sizeof(*ctx); n++) {
        p[n] = 0;
    }
    ctx->cam_deadzone_x = R01_CAM_DEADZONE_X_DEFAULT;
    ctx->cam_deadzone_y = R01_CAM_DEADZONE_Y_DEFAULT;
    ctx->cam_axis_lock = R01_CAM_AXIS_BOTH;
    ctx->game_mode = R01_GAME_MODE_TOPDOWN;
    ctx->plat_gravity = R01_PLAT_GRAVITY_DEFAULT;
    ctx->plat_jump = R01_PLAT_JUMP_DEFAULT;
    ctx->plat_meter = R01_PLAT_METER_DEFAULT;
    ctx->player_move_mul = 1;
    ctx->player_idle_state = 0xFFu;
    ctx->player_crouch_state = 0xFFu;
    ctx->player_jump_state = 0xFFu;
    for (i = 0; i < 8u; i++) {
        ctx->player_walk_state[i] = 0xFFu;
    }
}

void r01_pad_poll(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    ctx->pad_prev = ctx->pad;
    ctx->pad = *R01_PAD0;
}

uint8_t r01_pad_down(const R01GameCtx *ctx, uint8_t mask) {
    if (!ctx) {
        return 0;
    }
    return (uint8_t)((ctx->pad & mask) != 0u);
}

uint8_t r01_player_moving_x(const R01GameCtx *ctx) {
    if (!ctx) {
        return 0;
    }
    return (uint8_t)((ctx->pad & (R01_PAD_LEFT | R01_PAD_RIGHT)) != 0u);
}

void r01_player_set_move_mul(R01GameCtx *ctx, uint8_t mul) {
    if (!ctx) {
        return;
    }
    ctx->player_move_mul = mul ? mul : 1u;
}

void r01_camera_set_deadzone(R01GameCtx *ctx, uint8_t dx, uint8_t dy) {
    if (!ctx) {
        return;
    }
    ctx->cam_deadzone_x = dx;
    ctx->cam_deadzone_y = dy;
}

void r01_camera_disable_deadzone(R01GameCtx *ctx) {
    r01_camera_set_deadzone(ctx, 0, 0);
}

void r01_camera_set_axis_lock(R01GameCtx *ctx, uint8_t mode) {
    if (!ctx) {
        return;
    }
    ctx->cam_axis_lock = mode;
}

void r01_game_set_mode(R01GameCtx *ctx, uint8_t mode) {
    if (!ctx) {
        return;
    }
    ctx->game_mode = mode;
}

void r01_solid_pattern_add(R01GameCtx *ctx, uint8_t bank, uint8_t tile) {
    uint8_t n;
    if (!ctx) {
        return;
    }
    n = ctx->solid_pat_count;
    if (n >= 64u) {
        return;
    }
    ctx->solid_pat_bank[n] = bank;
    ctx->solid_pat_tile[n] = tile;
    ctx->solid_pat_count = (uint8_t)(n + 1u);
}

void r01_platformer_set_gravity(R01GameCtx *ctx, uint8_t units) {
    if (ctx) {
        ctx->plat_gravity = units;
    }
}

void r01_platformer_set_jump(R01GameCtx *ctx, uint8_t impulse) {
    if (ctx) {
        ctx->plat_jump = impulse;
    }
}

void r01_platformer_set_meter(R01GameCtx *ctx, uint8_t px_per_meter) {
    if (ctx) {
        ctx->plat_meter = px_per_meter;
    }
}

void r01_bg0_set_wrap(R01GameCtx *ctx, uint8_t wrap_x, uint8_t wrap_y) {
    if (!ctx) {
        return;
    }
    ctx->bg0_wrap_x = wrap_x;
    ctx->bg0_wrap_y = wrap_y;
}

void r01_bg0_set_clip_to_bg1(R01GameCtx *ctx, uint8_t enable) {
    if (ctx) {
        ctx->bg0_clip_bg1 = enable;
    }
}

void r01_bgm_play(R01GameCtx *ctx, uint8_t track) {
    if (ctx) {
        ctx->bgm_track = track;
    }
}

void r01_bgm_stop(R01GameCtx *ctx) {
    if (ctx) {
        ctx->bgm_track = 0;
    }
}

void r01_player_anim_set_idle_state(R01GameCtx *ctx, uint8_t entity_state_idx) {
    if (ctx) {
        ctx->player_idle_state = entity_state_idx;
    }
}

void r01_player_anim_set_walk_all(R01GameCtx *ctx, uint8_t entity_state_idx) {
    uint8_t i;
    if (!ctx) {
        return;
    }
    for (i = 0; i < 8u; i++) {
        ctx->player_walk_state[i] = entity_state_idx;
    }
}

void r01_player_anim_set_crouch_state(R01GameCtx *ctx, uint8_t entity_state_idx) {
    if (ctx) {
        ctx->player_crouch_state = entity_state_idx;
    }
}

void r01_player_anim_set_jump_state(R01GameCtx *ctx, uint8_t entity_state_idx) {
    if (ctx) {
        ctx->player_jump_state = entity_state_idx;
    }
}

void r01_player_anim_set_frame_delay(R01GameCtx *ctx, uint8_t ticks) {
    if (ctx) {
        ctx->player_anim_delay_override = ticks;
    }
}
