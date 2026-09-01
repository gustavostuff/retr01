#include "retr01_sim/board.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "sst39sf040.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

#ifdef R01_TEST_CART
#define R01S_TEST_CART R01_TEST_CART
#else
#define R01S_TEST_CART "../output/test.retr01"
#endif

static int write_tmp_cart(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01S_TEST_CART;
    R01sBoard board;
    R01sIslandBuilder builder;
    R01sIslandGroup *group;
    R01sBoard *b;
    uint8_t bad[64];

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");

    /* Reject format_ver 1. */
    memset(bad, 0, sizeof(bad));
    memcpy(bad, "retr01", 6);
    bad[6] = 1;
    bad[7] = 1;
    expect_true(write_tmp_cart("bad_fmt1.retr01", bad, sizeof(bad)) == 0, "write bad cart");
    expect_true(r01s_board_load_cart(b, "bad_fmt1.retr01") != 0, "reject format_ver 1");

    expect_true(r01s_board_load_cart(b, path) == 0, "load studio cart");
    expect_true(b->cart_loaded, "cart loaded flag");

    expect_true(r01s_board_cart_inserted(b), "cart inserted at boot");
    expect_true(r01s_board_select_cart(b, path) != 0, "reject select while inserted");
    r01s_board_toggle_cart(b);
    expect_true(!r01s_board_cart_inserted(b), "cart removed");
    expect_true(r01s_board_select_cart(b, path) == 0, "select cart while out");
    expect_true(r01s_board_cart_path(b) != NULL, "cart path set");
    expect_true(strcmp(r01s_board_cart_path(b), path) == 0, "cart path matches");

    expect_true(b->cart_format_ver == R01S_CART_FORMAT_VER, "format_ver 2");
    expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, 0) == 'r', "cart magic r");
    expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, 1) == 'e', "cart magic e");
    expect_true(b->cart_off_prg != 0, "PRG offset");
    expect_true(b->cart_len_prg == R01S_CART_PRG_BYTES, "PRG 32KB");
    expect_true(b->cart_off_chr != 0, "CHR offset");
    expect_true(b->cart_off_map_screen0 != 0, "MAP offset");

    expect_true(r01s_oam_tile_off_screen(-8, 0), "oam off left");
    expect_true(!r01s_oam_tile_off_screen(-4, 10), "oam partial on");

    /* AABB helpers callable after cart meta refresh. */
    (void)r01s_board_player_aabb_ok(b, b->cart_start_col * 128 + 60, b->cart_start_row * 120 + 56);

    r01s_island_builder_shutdown(&builder);
    return test_done("test_cart");
}
