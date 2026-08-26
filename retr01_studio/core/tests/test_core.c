#include "retr01_studio/chr_pack.h"
#include "retr01_studio/cart.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

int main(void) {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01PlayState pl;
    char err[128];

    if (!p) {
        fprintf(stderr, "FAIL: oom\n");
        return 1;
    }

    r01_project_init(p, "test");
    expect_true(p->worlds[0].screen_count == R01_DEFAULT_GRID * R01_DEFAULT_GRID, "9 screens default");
    expect_true(p->worlds[0].screens[2].col == 2 && p->worlds[0].screens[2].row == 0, "start screen 2,0");
    expect_true(p->active_screen == 2, "active start 2,0");
    {
        int i;
        for (i = 0; i < p->worlds[0].screen_count; i++) {
            p->worlds[0].screens[i].present = 1;
        }
    }

    expect_true(r01_play_start(&pl, p), "play start");
    expect_true(pl.active, "play active");
    expect_true(pl.player_x == R01_START_COL * R01_SCREEN_PX_W + R01_SCREEN_PX_W - R01_PLAY_PLAYER_W -
                                    R01_PLAY_SPAWN_MARGIN_RIGHT,
                "spawn margin right");
    expect_true(pl.player_y == R01_START_ROW * R01_SCREEN_PX_H + R01_SCREEN_PX_H - R01_PLAY_PLAYER_H -
                                    R01_PLAY_SPAWN_MARGIN_BOTTOM,
                "spawn margin bottom");
    expect_true(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2 &&
                    pl.cam_y == pl.player_y + R01_PLAY_PLAYER_H / 2 - R01_SCREEN_PX_H / 2,
                "cam already smooth-follows on start");

    r01_play_tick(&pl, p, 0, 0);
    expect_true(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "idle keeps follow");

    r01_play_tick(&pl, p, 1, 0);
    expect_true(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "smooth cam");

    expect_true(r01_play_button(&pl, p, R01_PLAY_BTN_X), "warp X");
    expect_true(pl.player_x / R01_SCREEN_PX_W == 0, "warp col 0");
    expect_true(pl.player_x % R01_SCREEN_PX_W ==
                    R01_SCREEN_PX_W - R01_PLAY_PLAYER_W - R01_PLAY_SPAWN_MARGIN_RIGHT,
                "warp keeps right margin");
    expect_true(pl.player_y % R01_SCREEN_PX_H ==
                    R01_SCREEN_PX_H - R01_PLAY_PLAYER_H - R01_PLAY_SPAWN_MARGIN_BOTTOM,
                "warp keeps bottom margin");
    expect_true(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2 &&
                    pl.cam_y == pl.player_y + R01_PLAY_PLAYER_H / 2 - R01_SCREEN_PX_H / 2,
                "warp cam already follows");

    expect_true(r01_project_save_json(p, "test_project.json", err, sizeof(err)) == 0, "save json");
    memset(p, 0, sizeof(*p));
    expect_true(r01_project_load_json(p, "test_project.json", err, sizeof(err)) == 0, "load json");
    expect_true(p->worlds[0].screen_count == R01_DEFAULT_GRID * R01_DEFAULT_GRID, "reload screens");

    expect_true(r01_cart_write(p, "test_cart.retr01", err, sizeof(err)) == 0, "cart write");
    {
        FILE *f = fopen("test_cart.retr01", "rb");
        char magic[6];
        if (f && fread(magic, 1, 6, f) == 6) {
            expect_true(memcmp(magic, "retr01", 6) == 0, "cart magic");
        } else {
            expect_true(0, "cart magic");
        }
        if (f) {
            fclose(f);
        }
    }

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        free(p);
        return 1;
    }
    free(p);
    puts("ok");
    return 0;
}
