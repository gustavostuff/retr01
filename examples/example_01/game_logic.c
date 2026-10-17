/* Author game logic. Created once. Never overwritten. */
#include <r01_engine.h>
#include "r01_entity_ids.h"
#include "r01_warp_ids.h"

#define SLIME_JUMP_MIN 30
#define SLIME_JUMP_MAX 60
#define SLIME_JUMP_PX 16

static uint8_t s_ready;
static uint16_t s_wait[16];
static uint8_t s_up[16];
static uint16_t s_rng = 0xACE1u;

static void init_camera_and_bg0(R01GameCtx *ctx) {
    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);
    r01_camera_set_deadzone(ctx, 32, 70);
    r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);
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

static uint16_t rnd16(void) {
    s_rng = (uint16_t)(s_rng * 2053u + 13849u);
    return s_rng;
}

/* Inclusive SLIME_JUMP_MIN..MAX. Rejection, no libc division. */
static uint16_t slime_jump_gap(void) {
    uint16_t span = (uint16_t)(SLIME_JUMP_MAX - SLIME_JUMP_MIN);
    uint16_t mask = 1u;
    uint16_t r;
    while (mask < span) {
        mask = (uint16_t)((mask << 1) | 1u);
    }
    do {
        r = rnd16() & mask;
    } while (r > span);
    return (uint16_t)(SLIME_JUMP_MIN + r);
}

static int slime_fits(int x, int y, int hx, int hy, uint8_t hw, uint8_t hh) {
    return r01_world_aabb_ok(x + hx, y + hy, hw, hh);
}

static void slime_ai_tick(R01GameCtx *ctx) {
    uint8_t i;
    uint8_t n;
    if (!ctx) {
        return;
    }
    n = r01_entity_count();
    if (n > 16u) {
        n = 16u;
    }
    if (!s_ready) {
        s_rng ^= (uint16_t)ctx->player_x;
        s_rng ^= (uint16_t)(ctx->player_y << 1);
        if (s_rng == 0u) {
            s_rng = 0xACE1u;
        }
        for (i = 0; i < n; i++) {
            s_wait[i] = slime_jump_gap();
            s_up[i] = 0;
        }
        s_ready = 1;
    }
    for (i = 0; i < n; i++) {
        uint16_t x;
        uint16_t y;
        uint8_t grounded;
        if (r01_entity_type(i) != (uint8_t)R01_ENT_SLIME) {
            continue;
        }
        r01_entity_get_pos(i, &x, &y);
        {
            uint8_t t = r01_entity_type(i);
            int hx = (int)r01_ent_hx[t];
            int hy = (int)r01_ent_hy[t];
            uint8_t hw = r01_ent_hw[t];
            uint8_t hh = r01_ent_hh[t];
            if (s_wait[i] > 0u) {
                s_wait[i]--;
            }
            if (slime_fits((int)x + 1, (int)y, hx, hy, hw, hh)) {
                x++;
            }
            grounded = (uint8_t)!slime_fits((int)x, (int)y + 1, hx, hy, hw, hh);
            if (s_wait[i] == 0u && grounded) {
                s_up[i] = (uint8_t)SLIME_JUMP_PX;
                s_wait[i] = slime_jump_gap();
                grounded = 0;
            }
            if (s_up[i] > 0u) {
                if (y > 0u && slime_fits((int)x, (int)y - 1, hx, hy, hw, hh)) {
                    y--;
                    s_up[i]--;
                } else {
                    s_up[i] = 0;
                }
                grounded = 0;
            } else if (!grounded) {
                y++;
            }
        }
        r01_entity_set_pos(i, x, y);
        r01_entity_set_state(i, grounded ? 0u : 1u);
    }
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
    slime_ai_tick(ctx);
}

void r01_game_on_vblank(R01GameCtx *ctx) {
    (void)ctx;
}
