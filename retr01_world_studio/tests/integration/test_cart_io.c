#include "test_harness.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/screen.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void make_test_screen(retr01_screen_t *s, uint8_t col, uint8_t row, uint8_t tile)
{
    retr01_screen_clear(s);
    s->col = col;
    s->row = row;
    s->flags = 0;
    memset(s->tiles, tile, sizeof(s->tiles));
}

TEST(cart_roundtrip)
{
    retr01_cart_t cart;
    retr01_cart_t loaded;
    retr01_map_build_screen_t screens[1];
    retr01_map_build_world_t worlds[1];
    uint8_t *map_blob = NULL;
    size_t map_len = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/retr01_cart_test.retr01", RETR01_FIXTURES_DIR);

    retr01_cart_init(&cart);
    retr01_cart_init(&loaded);

    make_test_screen(&screens[0].screen, 0, 0, 2);
    worlds[0].desc.grid_w = 1;
    worlds[0].desc.grid_h = 1;
    worlds[0].desc.empty_off = 0;
    worlds[0].screens = screens;
    worlds[0].screen_count = 1;

    ASSERT_EQ(retr01_map_build(worlds, 1, &map_blob, &map_len), 0);

    cart.prg = (uint8_t *)malloc(4);
    ASSERT(cart.prg != NULL);
    cart.prg[0] = 0xEA;
    cart.prg_size = 4;
    cart.chr_size = 0;
    cart.map = map_blob;
    cart.map_size = map_len;
    cart.world_count = 1;

    ASSERT_EQ(retr01_cart_write_file(path, &cart), 0);
    ASSERT_EQ(retr01_cart_load_file(path, &loaded), 0);
    ASSERT_EQ(loaded.prg_size, 4);
    ASSERT_EQ(loaded.map_size, map_len);
    ASSERT(memcmp(loaded.map, map_blob, map_len) == 0);

    remove(path);
    retr01_cart_free(&cart);
    retr01_cart_free(&loaded);
}

TEST_RUNNER_BEGIN("test_cart_io")
RUN_TEST_RC(cart_roundtrip);
TEST_RUNNER_END()
