/* User game logic. Created once by Studio export. Never overwritten. */
#include "include/r01_engine.h"

static void init_camera_and_bg0(R01GameCtx *ctx) {
    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);
    r01_camera_set_deadzone(ctx, 32, 70);
    // r01_camera_disable_deadzone(ctx);
    // r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
}

static void init_platformer(R01GameCtx *ctx) {
    r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);
    r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT);
    r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);
    r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);
}

static void init_player_anim(R01GameCtx *ctx) {
    r01_player_anim_set_idle_state(ctx, 0);
    r01_player_anim_set_walk_all(ctx, 1);
    r01_player_anim_set_crouch_state(ctx, 3);
    r01_player_anim_set_jump_state(ctx, 2);
}

void r01_custom_on_init(R01GameCtx *ctx) {
    init_camera_and_bg0(ctx);
    init_platformer(ctx);
    init_player_anim(ctx);
    r01_bgm_play(ctx, 1);
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    (void)ctx;
}

void r01_custom_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
