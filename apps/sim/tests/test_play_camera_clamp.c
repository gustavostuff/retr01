#include "as6c62256.h"
#include "retr01_sim/board.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

#ifndef R01S_DEFAULT_CART
#define R01S_DEFAULT_CART "../output/test_2.retr01"
#endif

/*
 * Host Play must clamp camera to the present BG1 bbox (emu cam_max_*).
 * Without that, a tall player spawn can origin-row past the map and wipe VRAM.
 */
int main(int argc, char **argv) {
    R01sBoard board;
    R01sIslandBuilder builder;
    const char *cart = argc > 1 ? argv[1] : R01S_DEFAULT_CART;
    int nz = 0;
    int i;
    int present = 0;

    memset(&board, 0, sizeof(board));
    memset(&builder, 0, sizeof(builder));
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    expect_true(r01s_board_load_cart(&board, cart) == 0, "load cart");
    r01s_board_load_bg0(&board);
    (void)r01s_board_load_camera_2x2(&board, (int)board.cart_start_col, (int)board.cart_start_row);
    for (i = 0; i < 4; i++) {
        present += board.vram_slot_present[i] ? 1 : 0;
    }
    expect_true(present > 0, "start 2x2 has present slots");

    expect_true(r01s_play_start(&board) != 0, "play_start");
    expect_true(board.play.cam_y <= board.cam_max_y, "cam_y clamped to present bbox");
    expect_true(board.play.cam_x <= board.cam_max_x, "cam_x clamped to present bbox");
    expect_true(board.play.origin_row <= board.cam_max_y / R01S_BG_SCREEN_PX_H,
                "origin_row within present rows");

    present = 0;
    for (i = 0; i < 4; i++) {
        present += board.vram_slot_present[i] ? 1 : 0;
    }
    for (i = 0; i < 1920; i++) {
        if (r01s_as6c62256_peek(&board.vram, (uint16_t)i) != 0) {
            nz++;
        }
    }
    expect_true(present > 0, "play_start kept present VRAM slots");
    expect_true(nz > 0, "play_start kept non-zero BG tiles");

    return test_done("test_play_camera_clamp");
}
