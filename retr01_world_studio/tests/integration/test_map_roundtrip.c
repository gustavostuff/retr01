#include "test_harness.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/screen.h"

#include <stdlib.h>
#include <string.h>

TEST(three_screen_world)
{
    retr01_map_build_screen_t screens[3];
    retr01_map_build_world_t worlds[1];
    uint8_t *map = NULL;
    size_t map_len = 0;
    retr01_cart_t cart;
    retr01_screen_t loaded;

    retr01_cart_init(&cart);

    for (int i = 0; i < 3; i++) {
        retr01_screen_clear(&screens[i].screen);
        screens[i].screen.col = (uint8_t)i;
        screens[i].screen.row = 0;
        screens[i].screen.flags = 0;
        memset(screens[i].screen.tiles, (uint8_t)(i + 1), RETR01_SCREEN_TILE_BYTES);
    }

    worlds[0].desc.grid_w = 3;
    worlds[0].desc.grid_h = 1;
    worlds[0].desc.empty_off = 0;
    worlds[0].screens = screens;
    worlds[0].screen_count = 3;

    ASSERT_EQ(retr01_map_build(worlds, 1, &map, &map_len), 0);
    ASSERT(map_len > 30);

    cart.map = map;
    cart.map_size = map_len;
    cart.world_count = 1;

    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 1, 0, &loaded), 0);
    ASSERT_EQ(loaded.tiles[0], 2);
    ASSERT_EQ(loaded.tiles[100], 2);

    free(map);
}

TEST_RUNNER_BEGIN("test_map_roundtrip")
RUN_TEST_RC(three_screen_world);
TEST_RUNNER_END()
