#include "r01_engine.h"

#include <stdio.h>
#include <string.h>

uint8_t r01_host_io[256];

static int fails;

static void expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        fails++;
    }
}

static void wipe_io(void) {
    memset(r01_host_io, 0, sizeof(r01_host_io));
}

int main(void) {
    R01GameCtx ctx;
    uint8_t i;

    wipe_io();
    r01_game_ctx_init(NULL);
    r01_game_on_init(NULL);
    r01_game_on_tick(NULL);
    r01_game_on_vblank(NULL);
    r01_game_play_tick(NULL);
    r01_pad_down(NULL, R01_PAD_X);
    r01_player_moving_x(NULL);
    r01_solid_pattern_add(NULL, 0, 1);

    r01_game_ctx_init(&ctx);
    expect(ctx.cam_deadzone_x == R01_CAM_DEADZONE_X_DEFAULT, "init deadzone x");
    expect(ctx.cam_deadzone_y == R01_CAM_DEADZONE_Y_DEFAULT, "init deadzone y");
    expect(ctx.game_mode == R01_GAME_MODE_TOPDOWN, "init topdown");
    expect(ctx.player_move_mul == 1, "init move mul");
    expect(ctx.player_idle_state == 0xFFu, "init idle unmapped");
    expect(ctx.bgm_track == 0, "init no bgm");
    expect(ctx.solid_pat_count == 0, "init no solids");

    r01_game_on_init(&ctx);
    expect(ctx.bg0_wrap_x == R01_BG0_WRAP_ON && ctx.bg0_wrap_y == R01_BG0_WRAP_ON, "wrap on");
    expect(ctx.cam_deadzone_x == 32 && ctx.cam_deadzone_y == 70, "deadzone 32x70");
    expect(ctx.game_mode == R01_GAME_MODE_PLATFORMER, "platformer");
    expect(ctx.plat_gravity == R01_PLAT_GRAVITY_DEFAULT, "gravity default");
    expect(ctx.plat_jump == R01_PLAT_JUMP_DEFAULT, "jump default");
    expect(ctx.plat_meter == R01_PLAT_METER_DEFAULT, "meter default");
    expect(ctx.player_idle_state == 0, "idle state 0");
    expect(ctx.player_walk_state[0] == 1 && ctx.player_walk_state[7] == 1, "walk all 1");
    expect(ctx.player_crouch_state == 3, "crouch 3");
    expect(ctx.player_jump_state == 2, "jump 2");
    expect(ctx.solid_pat_count == 0, "solids from Set Solid not C");
    expect(ctx.bgm_track == 1, "bgm track 1");

    ctx.player_move_mul = 1;
    ctx.player_anim_delay_override = 0;
    ctx.pad = R01_PAD_X | R01_PAD_RIGHT;
    r01_game_on_tick(&ctx);
    expect(ctx.player_move_mul == 2, "turbo mul");
    expect(ctx.player_anim_delay_override == 5, "turbo anim delay");

    ctx.player_move_mul = 1;
    ctx.player_anim_delay_override = 0;
    ctx.pad = R01_PAD_RIGHT;
    r01_game_on_tick(&ctx);
    expect(ctx.player_move_mul == 1, "move without X is not turbo");
    expect(ctx.player_anim_delay_override == 0, "authored delay when not turbo");

    ctx.player_move_mul = 1;
    ctx.player_anim_delay_override = 0;
    ctx.pad = R01_PAD_X;
    r01_game_on_tick(&ctx);
    expect(ctx.player_move_mul == 1, "X without move is not turbo");

    ctx.pad = R01_PAD_RIGHT;
    r01_game_on_tick(&ctx);
    expect(ctx.player_move_mul == 1, "move without X is not turbo");

    r01_game_on_vblank(&ctx);

    expect(r01_pad_down(&ctx, R01_PAD_RIGHT) == 1, "pad down right");
    expect(r01_player_moving_x(&ctx) == 1, "moving x");
    ctx.pad = 0;
    expect(r01_player_moving_x(&ctx) == 0, "not moving x");

    r01_camera_disable_deadzone(&ctx);
    expect(ctx.cam_deadzone_x == 0 && ctx.cam_deadzone_y == 0, "disable deadzone");
    r01_camera_set_axis_lock(&ctx, R01_CAM_AXIS_H);
    expect(ctx.cam_axis_lock == R01_CAM_AXIS_H, "axis lock H");
    r01_bgm_stop(&ctx);
    expect(ctx.bgm_track == 0, "bgm stop");
    r01_player_set_move_mul(&ctx, 0);
    expect(ctx.player_move_mul == 1, "mul 0 clamps to 1");

    ctx.solid_pat_count = 0;
    for (i = 0; i < 64u; i++) {
        r01_solid_pattern_add(&ctx, i, (uint8_t)(i + 1u));
    }
    r01_solid_pattern_add(&ctx, 9, 9);
    expect(ctx.solid_pat_count == 64, "solid cap 64");
    expect(ctx.solid_pat_bank[63] == 63 && ctx.solid_pat_tile[63] == 64, "last solid kept");

    r01_host_io[0x60] = R01_PAD_LEFT;
    r01_pad_poll(&ctx);
    expect(ctx.pad == R01_PAD_LEFT, "pad poll");
    r01_host_io[0x60] = R01_PAD_RIGHT;
    r01_pad_poll(&ctx);
    expect(ctx.pad_prev == R01_PAD_LEFT, "pad prev");
    expect(ctx.pad == R01_PAD_RIGHT, "pad now");

    r01_map_seek(0x123456u);
    expect(r01_host_io[0x90] == 0x56 && r01_host_io[0x91] == 0x34 && r01_host_io[0x92] == 0x12, "map seek");
    r01_host_io[0x93] = 0xAB;
    expect(r01_map_read() == 0xAB, "map read");

    r01_oam_reset();
    expect(r01_host_io[0x20] == 0, "oam addr 0");
    r01_oam_write(10, 3, 0x40, 20);
    expect(r01_host_io[0x21] == 20, "oam last write x");

    r01_host_io[0x01] = 0x80;
    r01_ppu_wait_vblank();

    r01_game_play_reset();
    r01_game_ctx_init(&ctx);
    r01_game_on_init(&ctx);
    ctx.player_x = 40;
    ctx.player_y = 40;
    ctx.pad = R01_PAD_RIGHT;
    r01_game_on_tick(&ctx);
    r01_game_play_tick(&ctx);
    expect(ctx.player_x > 40, "play tick walks right");
    expect(r01_host_io[0x02] == (uint8_t)(ctx.cam_x & 0x7Fu), "scroll x publish");

    r01_game_play_reset();
    r01_game_ctx_init(&ctx);
    r01_game_on_init(&ctx);
    ctx.player_x = 40;
    ctx.player_y = 40;
    ctx.player_move_mul = 1;
    ctx.pad = R01_PAD_X | R01_PAD_RIGHT;
    r01_game_on_tick(&ctx);
    r01_game_play_tick(&ctx);
    expect(ctx.player_move_mul == 2, "turbo before play tick");
    expect(ctx.player_x > 41, "turbo walks faster than 1 px");

    if (fails) {
        fprintf(stderr, "%d test(s) failed\n", fails);
        return 1;
    }
    puts("ok sdk game_logic");
    return 0;
}
