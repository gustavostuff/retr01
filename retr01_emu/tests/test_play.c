#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"

#include <stdio.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../rom/test.retr01"
#endif

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eMachine m;
    char err[256];
    int before_x;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    before_x = m.play.player_x;
    m.io.pad0 = R01E_PAD_RIGHT;
    r01e_play_tick(&m);
    if (m.play.player_x != before_x + 1) {
        fprintf(stderr, "FAIL move right: %d -> %d\n", before_x, m.play.player_x);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (m.play.cam_x != m.play.player_x + m.play.player_w / 2 - R01E_SCREEN_PX_W / 2) {
        fprintf(stderr, "FAIL camera follow\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok play spawn=%d,%d move=%d\n", before_x, m.play.player_y, m.play.player_x);
    r01e_machine_shutdown(&m);
    return 0;
}
