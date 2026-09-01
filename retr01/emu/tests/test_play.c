#include "retr01_emu/machine.h"
#include "retr01_emu/cart.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"
#include "r01_play_camera.h"

#include <stdio.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../../output/test.retr01"
#endif

static int cart_player_spawn(const R01eMachine *m, int *out_x, int *out_y) {
    R01eWorldView wv;
    const uint8_t *insts;
    int ii;

    if (!m || r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
        return 0;
    }
    if (wv.player_entity == R01E_CART_PLAYER_ENTITY_NONE || wv.player_entity >= wv.entity_type_count ||
        wv.entity_inst_count < 1) {
        return 0;
    }
    insts = r01e_cart_ptr(&m->cart, wv.base + wv.off_entity_insts,
                          (size_t)wv.entity_inst_count * R01E_CART_INSTANCE_SIZE);
    if (!insts) {
        return 0;
    }
    for (ii = 0; ii < (int)wv.entity_inst_count; ii++) {
        const uint8_t *irec = insts + (size_t)ii * R01E_CART_INSTANCE_SIZE;
        if (irec[0] != wv.player_entity) {
            continue;
        }
        if (out_x) {
            *out_x = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
        }
        if (out_y) {
            *out_y = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
        }
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eMachine m;
    char err[256];
    int spawn_x;
    int spawn_y;
    int expect_x;
    int expect_y;
    int expect_cam_x;
    int expect_cam_y;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }
    if (!cart_player_spawn(&m, &expect_x, &expect_y)) {
        fprintf(stderr, "FAIL cart has no player instance\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    spawn_x = m.play.player_x;
    spawn_y = m.play.player_y;
    if (spawn_x != expect_x || spawn_y != expect_y) {
        fprintf(stderr, "FAIL spawn at instance: got %d,%d expected %d,%d\n", spawn_x, spawn_y, expect_x,
                expect_y);
        r01e_machine_shutdown(&m);
        return 1;
    }
    {
        expect_cam_x = m.play.cam_x;
        expect_cam_y = m.play.cam_y;
        r01_play_camera_update(&expect_cam_x, &expect_cam_y, spawn_x, spawn_y, m.play.player_w, m.play.player_h,
                               R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, m.play.cam_deadzone_x, m.play.cam_deadzone_y,
                               R01_PLAY_CAM_AXIS_BOTH);
        if (m.play.cam_x != expect_cam_x || m.play.cam_y != expect_cam_y) {
            fprintf(stderr, "FAIL camera at spawn: got %d,%d expected %d,%d\n", m.play.cam_x, m.play.cam_y,
                    expect_cam_x, expect_cam_y);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    /* Leaving the dead zone scrolls the camera. */
    m.play.player_x = spawn_x + 8;
    m.play.player_y = spawn_y;
    m.io.pad0 = 0;
    r01e_play_tick(&m);
    {
        expect_cam_x = m.play.cam_x;
        expect_cam_y = m.play.cam_y;
        r01_play_camera_update(&expect_cam_x, &expect_cam_y, m.play.player_x, m.play.player_y, m.play.player_w,
                               m.play.player_h, R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, m.play.cam_deadzone_x,
                               m.play.cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
        if (m.play.cam_x != expect_cam_x || m.play.cam_y != expect_cam_y) {
            fprintf(stderr, "FAIL camera follow after deadzone exit: got %d,%d expected %d,%d\n", m.play.cam_x,
                    m.play.cam_y, expect_cam_x, expect_cam_y);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    printf("ok play spawn=%d,%d cam=%d\n", spawn_x, spawn_y, m.play.cam_x);
    r01e_machine_shutdown(&m);
    return 0;
}
