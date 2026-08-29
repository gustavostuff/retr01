#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"
#include "r01_play_camera.h"

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
    int expect_cam_x;
    int expect_cam_y;

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
    {
        expect_cam_x = m.play.cam_x;
        expect_cam_y = m.play.cam_y;
        r01_play_camera_update(&expect_cam_x, &expect_cam_y, spawn_x, spawn_y, m.play.player_w, m.play.player_h,
                               R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, R01_PLAY_CAM_DEADZONE_X_DEFAULT,
                               R01_PLAY_CAM_DEADZONE_Y_DEFAULT, R01_PLAY_CAM_AXIS_BOTH);
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
                               m.play.player_h, R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, R01_PLAY_CAM_DEADZONE_X_DEFAULT,
                               R01_PLAY_CAM_DEADZONE_Y_DEFAULT, R01_PLAY_CAM_AXIS_BOTH);
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
