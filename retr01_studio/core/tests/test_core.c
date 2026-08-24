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
    expect_true(p->worlds[0].screens[4].col == 1 && p->worlds[0].screens[4].row == 1, "center screen");
    expect_true(p->active_screen == 4, "active start 1,1");
    {
        int i;
        for (i = 0; i < p->worlds[0].screen_count; i++) {
            p->worlds[0].screens[i].present = 1;
        }
    }

    expect_true(r01_play_start(&pl, p), "play start");
    expect_true(pl.active, "play active");
    expect_true(pl.player_x > 0 && pl.player_y > 0, "player placed");

    r01_play_tick(&pl, p, 1, 0);
    expect_true(pl.cam_x == pl.player_x + 4 - R01_SCREEN_PX_W / 2, "smooth cam");

    expect_true(r01_play_button(&pl, p, R01_PLAY_BTN_X), "warp X");
    expect_true(pl.player_x / R01_SCREEN_PX_W == 0, "warp col 0");

    expect_true(r01_project_save_json(p, "test_project.json", err, sizeof(err)) == 0, "save json");
    memset(p, 0, sizeof(*p));
    expect_true(r01_project_load_json(p, "test_project.json", err, sizeof(err)) == 0, "load json");
    expect_true(p->worlds[0].screen_count == R01_DEFAULT_GRID * R01_DEFAULT_GRID, "reload screens");

    expect_true(r01_cart_write(p, "test_cart.retr01", err, sizeof(err)) == 0, "cart write");
    {
        FILE *f = fopen("test_cart.retr01", "rb");
        char magic[6];
        if (f && fread(magic, 1, 6, f) == 6) {
            expect_true(memcmp(magic, "RETR01", 6) == 0, "cart magic");
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
