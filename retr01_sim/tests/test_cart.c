#include "retr01_sim/board.h"
#include "retr01_sim/island_builder.h"
#include "sst39sf040.h"
#include "test_common.h"

#include <stdio.h>

#ifdef R01_TEST_CART
#define R01S_TEST_CART R01_TEST_CART
#else
#define R01S_TEST_CART "../rom/test.retr01"
#endif

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01S_TEST_CART;
    R01sBoard board;
    R01sIslandBuilder builder;
    R01sIslandGroup *group;
    R01sBoard *b;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");

    expect_true(r01s_board_load_cart(b, path) == 0, "load studio cart");
    expect_true(b->cart_loaded, "cart loaded flag");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 0) == 'r', "cart magic r");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 1) == 'e', "cart magic e");
    expect_true(b->cart_off_prg != 0, "PRG offset");
    expect_true(b->cart_off_chr != 0, "CHR offset");
    expect_true(b->cart_off_map_screen0 != 0, "MAP offset");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_cart");
}
