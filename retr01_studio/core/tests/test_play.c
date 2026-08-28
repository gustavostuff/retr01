#include "test_harness.h"

#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01PlayState pl;
    int i;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "test");
    for (i = 0; i < p->worlds[0].screen_count; i++) {
        p->worlds[0].screens[i].present = 1;
    }

    p->worlds[0].default_screen = 2;
    r01_project_begin_play(p);
    EXPECT(r01_play_start(&pl, p), "play start");
    EXPECT(pl.active, "play active");
    EXPECT(pl.player_x == R01_PLAY_SPAWN_CENTER_X(2), "spawn center x on default screen");
    EXPECT(pl.player_y == R01_PLAY_SPAWN_CENTER_Y(0), "spawn center y on default screen");
    EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2 &&
               pl.cam_y == pl.player_y + R01_PLAY_PLAYER_H / 2 - R01_SCREEN_PX_H / 2,
           "camera centers on spawn");

    r01_play_tick(&pl, p, 0, 0);
    EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "idle keeps camera follow");

    {
        int before = pl.player_x;
        r01_play_tick(&pl, p, 1, 0);
        EXPECT(pl.player_x == before + 1, "move right");
        EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "camera follows move");
    }

    EXPECT(r01_play_button(&pl, p, R01_PLAY_BTN_X), "warp X");
    EXPECT(pl.player_x == R01_PLAY_SPAWN_CENTER_X(0), "warp to col 0 center");
    EXPECT(pl.player_y == R01_PLAY_SPAWN_CENTER_Y(0), "warp to row 0 center");

    EXPECT(r01_play_screen_index(&pl, &p->worlds[0]) == r01_world_find_screen(&p->worlds[0], 0, 0),
           "play_screen_index matches warp cell");

    {
        uint8_t r = 0, g = 0, b = 0;
        EXPECT(r01_play_sample_bg(p, &pl, 0, 0, &r, &g, &b) == 0, "sample bg in view");
    }

    r01_play_stop(&pl);
    EXPECT(!pl.active, "play stopped");

    free(p);
    TEST_EXIT();
}
