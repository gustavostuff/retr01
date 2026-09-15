/* User game logic - created once by Studio export; never overwritten. */
#include "include/r01_engine.h"

static void on_warp_x(R01GameCtx *ctx) {
    r01_player_warp(ctx, 0, 0);
}

void r01_custom_on_init(R01GameCtx *ctx) {
    r01_event_on_button(R01_BTN_X, on_warp_x);
    /* Examples:
     * r01_camera_set_deadzone(ctx, 32, 30); /* centered rect W x H */
     * r01_projectile_fire(ctx, 1, 0, 4);
     * r01_game_fade_start(ctx, R01_FADE_BLACK, R01_FADE_MAX);
     * r01_game_warp_by_id(ctx, "w_00");
     */
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    (void)ctx;
}

void r01_custom_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
