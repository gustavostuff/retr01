#include "as6c62256.h"
#include "avr128db28_s1.h"
#include "avr128db28_s2.h"
#include "retr01_sim/board.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "test_common.h"
#include "video_sink.h"

#include <stdio.h>
#include <string.h>

#ifndef R01S_DEFAULT_CART
#define R01S_DEFAULT_CART "../output/test.retr01"
#endif

/*
 * Host Play on the current example cart. The cart image is the game, not a
 * pin-level MAP smoke program. Play fills VRAM from the cart in the host.
 */
int main(int argc, char **argv) {
    R01sBoard board;
    R01sIslandBuilder builder;
    const char *cart = argc > 1 ? argv[1] : R01S_DEFAULT_CART;
    int i;
    int nz = 0;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");

    expect_true(r01s_board_load_cart(&board, cart) == 0, "load cart");
    expect_true(board.cart_format_ver == R01S_CART_FORMAT_VER, "format version");
    expect_true(r01s_play_start(&board) != 0, "Host Play start");
    expect_true(board.play.enabled, "play.enabled");
    expect_true(board.play.cam_x <= board.cam_max_x, "cam_x in present bbox");
    expect_true(board.play.cam_y <= board.cam_max_y, "cam_y in present bbox");

    for (i = 0; i < 1920; i++) {
        if (r01s_as6c62256_peek(&board.vram, (uint16_t)i) != 0) {
            nz++;
        }
    }
    expect_true(nz > 0, "BG VRAM non-empty after Host Play");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_host_play_smoke");
}
