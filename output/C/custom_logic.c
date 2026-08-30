/* User game logic — created once by Studio export; never overwritten. */
#include "include/r01_engine.h"

static int s_face = R01_PLAYER_FACE_RIGHT;

static void on_fire(R01GameCtx *ctx) {
    r01_projectile_fire(ctx, 1, 0, 4);
}

static void on_cycle_face(R01GameCtx *ctx) {
    s_face = (s_face + 1) % 4;
    r01_player_default_face_set(ctx, s_face);
}

void r01_custom_on_init(R01GameCtx *ctx) {
    /* player entity: state 0 = idle, state 1 = Walk (used for all 8 dirs for now) */
    r01_player_anim_set_idle_state(ctx, 0);
    r01_player_anim_set_walk_all(ctx, 1);
    r01_player_default_face_set(ctx, R01_PLAYER_FACE_RIGHT);
    r01_entity_state_frame_delay_set(ctx, 0, 10);
    r01_entity_state_frame_delay_set(ctx, 1, 4);
    r01_camera_set_deadzone(ctx, 32, 30);

    r01_event_on_button(R01_BTN_X, on_fire);
    r01_event_on_button(R01_BTN_Y, on_cycle_face);
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    (void)ctx;
}

void r01_custom_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
