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
    // r01_camera_disable_deadzone(ctx);
    // r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    (void)ctx;
}

void r01_custom_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
