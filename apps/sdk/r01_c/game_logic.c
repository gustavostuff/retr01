/* Author game logic. Created once. Never overwritten. */
#include <r01_engine.h>
#include "r01_entity_ids.h"
#include "r01_warp_ids.h"

/* Catalog indexes: R01_ENT_* and R01_WARP_* from include/ (export overwrites). */

static void init_camera_and_bg0(R01GameCtx *ctx) {
    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);
    r01_camera_set_deadzone(ctx, 32, 70);
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

void r01_game_on_init(R01GameCtx *ctx) {
    init_camera_and_bg0(ctx);
    init_platformer(ctx);
    init_player_anim(ctx);
    r01_bgm_play(ctx, 1);
}

void r01_game_on_tick(R01GameCtx *ctx) {
    if (r01_pad_down(ctx, R01_PAD_X) && r01_player_moving_x(ctx)) {
        r01_player_set_move_mul(ctx, 2);
        r01_player_anim_set_frame_delay(ctx, 5);
    }
}

void r01_game_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
