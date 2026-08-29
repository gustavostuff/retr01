#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"

#include <stdio.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../output/test.retr01"
#endif

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eMachine m;
    char err[256];
    int spawn_x;
    int spawn_y;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    spawn_x = m.play.player_x;
    spawn_y = m.play.player_y;
    /* test.r01proj marks player type 0; first instance is at 204,180. */
    if (spawn_x != 204 || spawn_y != 180) {
        fprintf(stderr, "FAIL spawn at instance: got %d,%d\n", spawn_x, spawn_y);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (m.play.cam_x != spawn_x + m.play.player_w / 2 - R01E_SCREEN_PX_W / 2) {
        fprintf(stderr, "FAIL camera at spawn\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    /* Camera follows Play position after a free-space step. */
    m.play.player_x = spawn_x + 1;
    m.play.player_y = spawn_y;
    m.io.pad0 = 0;
    r01e_play_tick(&m);
    if (m.play.cam_x != m.play.player_x + m.play.player_w / 2 - R01E_SCREEN_PX_W / 2) {
        fprintf(stderr, "FAIL camera follow\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok play spawn=%d,%d cam=%d\n", spawn_x, spawn_y, m.play.cam_x);
    r01e_machine_shutdown(&m);
    return 0;
}
