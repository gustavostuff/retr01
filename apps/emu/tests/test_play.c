#include "retr01_emu/machine.h"
#include "retr01_emu/cart.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"
#include "r01_play_camera.h"

#include <stdio.h>


int main(int argc, char **argv) {
    const char *path;
    if (argc < 2 || !argv[1] || !argv[1][0]) {
        fprintf(stderr, "skip: provide cart path argv\n");
        return 77;
    }
    path = argv[1];
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
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    spawn_x = expect_x = m.play.player_x;
    spawn_y = expect_y = m.play.player_y;
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

    /*
     * Sparse hole: example_01 has no screen at (3,1). A bbox cam_max would allow a
     * viewport that covers it (BG0 show-through). Clamp must pull cam back onto present cells.
     */
    if (r01e_cart_has_screen(&m.cart, 0, 2, 1) && !r01e_cart_has_screen(&m.cart, 0, 3, 1)) {
        int x0, y0, x1, y1, c, r, ok;
        m.play.cam_x = 2 * R01E_SCREEN_PX_W + 64; /* peeks into col 3 */
        m.play.cam_y = 1 * R01E_SCREEN_PX_H;
        m.play.player_x = m.play.cam_x + 40;
        m.play.player_y = m.play.cam_y + 40;
        m.io.pad0 = 0;
        r01e_play_tick(&m);
        x0 = m.play.cam_x;
        y0 = m.play.cam_y;
        x1 = x0 + R01E_SCREEN_PX_W - 1;
        y1 = y0 + R01E_SCREEN_PX_H - 1;
        ok = 1;
        for (r = y0 / R01E_SCREEN_PX_H; r <= y1 / R01E_SCREEN_PX_H; r++) {
            for (c = x0 / R01E_SCREEN_PX_W; c <= x1 / R01E_SCREEN_PX_W; c++) {
                if (!r01e_cart_has_screen(&m.cart, 0, c, r)) {
                    ok = 0;
                }
            }
        }
        if (!ok) {
            fprintf(stderr, "FAIL cam viewport covers missing BG1 after clamp: cam=%d,%d\n", x0, y0);
            r01e_machine_shutdown(&m);
            return 1;
        }
        if (x1 / R01E_SCREEN_PX_W >= 3) {
            fprintf(stderr, "FAIL cam still peeks into missing col 3: cam=%d right=%d\n", x0, x1);
            r01e_machine_shutdown(&m);
            return 1;
        }
        printf("ok sparse-hole cam clamp cam=%d right_col=%d\n", x0, x1 / R01E_SCREEN_PX_W);
    }

    r01e_machine_shutdown(&m);
    return 0;
}
