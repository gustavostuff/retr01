/* User game logic - created once by Studio export; never overwritten. */
#include "include/r01_engine.h"

/* Home screen for the X-button warp. */
#define CUSTOM_WARP_HOME_COL 0
#define CUSTOM_WARP_HOME_ROW 0

static void on_warp_x(R01GameCtx *ctx) {
    r01_player_warp(ctx, CUSTOM_WARP_HOME_COL, CUSTOM_WARP_HOME_ROW);
}

void r01_custom_on_init(R01GameCtx *ctx) {
    r01_event_on_button(R01_BTN_X, on_warp_x);
    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);
    r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
    /* Examples:
     * r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
     * r01_camera_set_deadzone(ctx, 32, 30); /* centered rect W x H */
     * r01_projectile_fire(ctx, R01_AIM_X_RIGHT, R01_AIM_Y_NONE, R01_PROJ_SPEED_FAST);
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
