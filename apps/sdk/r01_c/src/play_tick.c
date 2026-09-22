#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"

static R01PlayPhysics s_phys;
static uint8_t s_phys_ready;

void r01_game_play_reset(void) {
    s_phys_ready = 0;
}

static int move_ok_open(void *user, int x, int y) {
    (void)user;
    (void)x;
    (void)y;
    return 1;
}

void r01_game_play_tick(R01GameCtx *ctx) {
    int px;
    int py;
    int dx = 0;
    int dy = 0;
    int adx = 0;
    int ady = 0;
    int cx;
    int cy;
    int jump;

    if (!ctx) {
        return;
    }
    if (!s_phys_ready) {
        r01_play_physics_init(&s_phys);
        s_phys_ready = 1;
    }
    r01_play_physics_set_mode(&s_phys, (int)ctx->game_mode);
    r01_play_physics_set_gravity(&s_phys, (int)ctx->plat_gravity);
    r01_play_physics_set_jump(&s_phys, (int)ctx->plat_jump);
    r01_play_physics_set_meter(&s_phys, (int)ctx->plat_meter);
    r01_play_physics_set_run_mul(&s_phys, (int)ctx->player_move_mul);

    if (ctx->pad & R01_PAD_RIGHT) {
        dx = 1;
    }
    if (ctx->pad & R01_PAD_LEFT) {
        dx = -1;
    }
    if (ctx->pad & R01_PAD_DOWN) {
        dy = 1;
    }
    if (ctx->pad & R01_PAD_UP) {
        dy = -1;
    }
    jump = (ctx->pad & R01_PAD_Y) != 0;

    px = (int)ctx->player_x;
    py = (int)ctx->player_y;
    r01_play_physics_tick(&s_phys, &px, &py, dx, dy, jump, move_ok_open, ctx, &adx, &ady);
    ctx->player_x = (uint16_t)px;
    ctx->player_y = (uint16_t)py;
    ctx->player_anim_moving = (uint8_t)((adx != 0) || (ady != 0));

    cx = (int)ctx->cam_x;
    cy = (int)ctx->cam_y;
    r01_play_camera_update(&cx, &cy, px, py, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H,
                           (int)ctx->cam_deadzone_x, (int)ctx->cam_deadzone_y, (int)ctx->cam_axis_lock);
    ctx->cam_x = (uint16_t)cx;
    ctx->cam_y = (uint16_t)cy;
    *R01_SCROLL_X = (uint8_t)(ctx->cam_x & 0x7Fu);
    *R01_SCROLL_Y = (uint8_t)(ctx->cam_y & 0x7Fu);
}
