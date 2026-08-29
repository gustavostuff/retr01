#include "test_harness.h"

#include <stdlib.h>

#include "retr01_studio/game_runtime.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"
#include "retr01_studio/warps.h"
#include "r01_play_camera.h"

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    R01GameCtx ctx;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "runtime_test");
    w = r01_project_active_world(p);
    r01_game_ctx_init(&ctx);
    ctx.player_x = 64;
    ctx.player_y = 64;

    {
        int slot = r01_projectile_fire(&ctx, 1, 0, 4);
        EXPECT(slot == 0, "projectile slot");
        EXPECT(r01_projectile_count_active(&ctx) == 1, "one active projectile");
        r01_projectile_tick(&ctx, w);
        EXPECT(ctx.projectiles[0].active == 1, "projectile still active after tick");
    }

    {
        r01_game_fade_start(&ctx, R01_FADE_BLACK, R01_FADE_MAX);
        EXPECT(r01_game_fade_active(&ctx), "fade active");
        while (!r01_game_fade_tick(&ctx)) {
        }
        EXPECT(ctx.fade_level == R01_FADE_MAX, "fade reached max");
        r01_game_fade_start(&ctx, R01_FADE_BLACK, 0);
        while (!r01_game_fade_tick(&ctx)) {
        }
        EXPECT(ctx.fade_level == 0, "fade cleared");
    }

    {
        ctx.cam_x = 0;
        ctx.cam_y = 0;
        ctx.player_x = 204;
        ctx.player_y = 180;
        r01_camera_set_deadzone(&ctx, 8, 8);
        r01_game_camera_snap(&ctx);
        EXPECT(ctx.cam_x == 204 - R01_SCREEN_PX_W / 2, "snap centers player from zero cam");
        EXPECT(ctx.cam_y == 180 - R01_SCREEN_PX_H / 2, "snap centers player from zero cam y");
    }

    {
        int cam_x = 100;
        int cam_y = 100;
        r01_play_camera_update(&cam_x, &cam_y, 200, 180, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W,
                               R01_SCREEN_PX_H, 32, 30, R01_PLAY_CAM_AXIS_BOTH);
        EXPECT(cam_x == 200 - 79, "32px centered deadzone scrolls at right edge");
        EXPECT(cam_y == 180 - 74, "30px centered deadzone scrolls at bottom edge");
    }

    {
        int cam_x = 100;
        int cam_y = 100;
        r01_play_camera_update(&cam_x, &cam_y, 160, 150, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W,
                               R01_SCREEN_PX_H, 32, 30, R01_PLAY_CAM_AXIS_BOTH);
        EXPECT(cam_x == 100, "origin inside centered deadzone leaves camera x");
        EXPECT(cam_y == 100, "origin inside centered deadzone leaves camera y");
    }

    {
        ctx.cam_x = 0;
        ctx.cam_y = 0;
        ctx.player_x = 64;
        ctx.player_y = 64;
        r01_camera_set_deadzone(&ctx, 0, 0);
        r01_camera_set_axis_lock(&ctx, R01_CAM_AXIS_V);
        r01_game_camera_update(&ctx);
        EXPECT(ctx.cam_y == 64 + R01_PLAY_PLAYER_H / 2 - R01_SCREEN_PX_H / 2, "vertical camera follow when X locked");
    }

    {
        r01_world_warp_entrance_add(w, 0, 0, 2, 2);
        r01_world_warp_exit_set(w, 0, 1, 0, 4, 4, 0);
        ctx.player_x = 16;
        ctx.player_y = 16;
        ctx.fade_level = 0;
        ctx.fade_target = 0;
        r01_game_warp_check(&ctx, w);
        EXPECT(ctx.player_x == R01_SCREEN_PX_W + 32, "warp moved player to exit tile");
    }

    free(p);
    TEST_EXIT();
}
