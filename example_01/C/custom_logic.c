/* User game logic - created once by Studio export; never overwritten. */
#include "include/r01_engine.h"

void r01_custom_on_init(R01GameCtx *ctx) {
    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);
    r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);
    r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT);
    r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);
    r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);
    r01_player_anim_set_crouch_state(ctx, 2);
    r01_camera_set_deadzone(ctx, 32, 70);
    // r01_camera_disable_deadzone(ctx);
    // r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    (void)ctx;
}

void r01_custom_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
