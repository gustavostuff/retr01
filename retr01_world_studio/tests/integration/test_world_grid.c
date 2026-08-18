#include "test_harness.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/project.h"

#include <stdio.h>
#include <string.h>

TEST(world_grid_2x2_export)
{
    retr01_project_t proj;
    retr01_cart_t cart;
    retr01_screen_t loaded;
    retr01_project_screen_t *ps;
    char cart_path[512];
    int x;
    int y;

    snprintf(cart_path, sizeof(cart_path), "%s/world_2x2.retr01", RETR01_FIXTURES_DIR);
    retr01_project_init_default(&proj, RETR01_PALETTE_V01_PATH);
    proj.grid_w = 2;
    proj.grid_h = 2;

    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            ps = retr01_project_ensure_screen(&proj, 0, (uint8_t)x, (uint8_t)y);
            ASSERT(ps != NULL);
            ASSERT(ps->canvas != NULL);
            memset(ps->canvas, (uint8_t)(1 + x + y), RETR01_CANVAS_BYTES);
            memset(ps->screen.tiles, (uint8_t)(1 + x + y), RETR01_SCREEN_TILE_BYTES);
        }
    }

    ASSERT_EQ(proj.screen_count, 4);
    ASSERT_EQ(retr01_project_export_retr01(&proj, cart_path), 0);
    retr01_cart_init(&cart);
    ASSERT_EQ(retr01_cart_load_file(cart_path, &cart), 0);

    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 1, 1, &loaded), 0);
    ASSERT_EQ(loaded.tiles[0], 3);
    ASSERT_EQ(loaded.col, 1);
    ASSERT_EQ(loaded.row, 1);

    retr01_cart_free(&cart);
    retr01_project_free(&proj);
}

TEST(parallax_flags_roundtrip)
{
    retr01_project_t proj;
    retr01_cart_t cart;
    retr01_screen_t loaded;
    retr01_project_screen_t *ps;
    char cart_path[512];

    snprintf(cart_path, sizeof(cart_path), "%s/world_parallax.retr01", RETR01_FIXTURES_DIR);
    retr01_project_init_default(&proj, RETR01_PALETTE_V01_PATH);
    ps = retr01_project_ensure_screen(&proj, 0, 1, 0);
    ASSERT(ps != NULL);
    ps->screen.flags = 1;
    memset(ps->screen.tiles, 4, RETR01_SCREEN_TILE_BYTES);

    ASSERT_EQ(retr01_project_export_retr01(&proj, cart_path), 0);
    retr01_cart_init(&cart);
    ASSERT_EQ(retr01_cart_load_file(cart_path, &cart), 0);
    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 1, 0, &loaded), 0);
    ASSERT_EQ(loaded.flags, 1);
    ASSERT_EQ(loaded.tiles[0], 4);

    retr01_cart_free(&cart);
    retr01_project_free(&proj);
}

TEST(two_worlds_export)
{
    retr01_project_t proj;
    retr01_cart_t cart;
    retr01_screen_t loaded;
    retr01_map_cell_t cells[8];
    int count = 0;
    char cart_path[512];

    snprintf(cart_path, sizeof(cart_path), "%s/two_worlds.retr01", RETR01_FIXTURES_DIR);
    retr01_project_init_default(&proj, RETR01_PALETTE_V01_PATH);
    memset(proj.screens[0].screen.tiles, 1, RETR01_SCREEN_TILE_BYTES);

    ASSERT(retr01_project_ensure_screen(&proj, 1, 0, 0) != NULL);
    memset(proj.screens[proj.active_screen].screen.tiles, 7, RETR01_SCREEN_TILE_BYTES);

    ASSERT_EQ(retr01_project_export_retr01(&proj, cart_path), 0);
    retr01_cart_init(&cart);
    ASSERT_EQ(retr01_cart_load_file(cart_path, &cart), 0);
    ASSERT_EQ(retr01_map_world_count(&cart), 2);

    ASSERT_EQ(retr01_map_list_cells(&cart, 0, cells, 8, &count), 0);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 0, 0, &loaded), 0);
    ASSERT_EQ(loaded.tiles[0], 1);

    ASSERT_EQ(retr01_map_list_cells(&cart, 1, cells, 8, &count), 0);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(retr01_map_load_screen(&cart, 1, 0, 0, &loaded), 0);
    ASSERT_EQ(loaded.tiles[0], 7);

    retr01_cart_free(&cart);
    retr01_project_free(&proj);
}

TEST_RUNNER_BEGIN("test_world_grid")
RUN_TEST_RC(world_grid_2x2_export);
RUN_TEST_RC(parallax_flags_roundtrip);
RUN_TEST_RC(two_worlds_export);
TEST_RUNNER_END()
