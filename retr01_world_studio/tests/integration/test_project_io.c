#include "test_harness.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/project.h"
#include "retr01/screen.h"

#include <stdio.h>
#include <string.h>

TEST(project_save_load_roundtrip)
{
    retr01_project_t a;
    retr01_project_t b;
    char path[512];

    snprintf(path, sizeof(path), "%s/project_roundtrip.r01proj", RETR01_FIXTURES_DIR);

    retr01_project_init_default(&a, RETR01_PALETTE_V01_PATH);
    snprintf(a.title, sizeof(a.title), "Roundtrip");
    memset(a.screens[0].screen.tiles, 2, sizeof(a.screens[0].screen.tiles));
    a.chr_used[0] = 3;

    ASSERT_EQ(retr01_project_save(&a, path), 0);
    ASSERT_EQ(retr01_project_load(&b, path), 0);
    ASSERT_EQ(strcmp(a.title, b.title), 0);
    ASSERT_EQ(b.screens[0].screen.tiles[0], 2);
    ASSERT_EQ(b.chr_used[0], 3);
}

TEST(project_export_cart)
{
    retr01_project_t proj;
    retr01_cart_t cart;
    retr01_screen_t loaded;
    char proj_path[512];
    char cart_path[512];

    snprintf(proj_path, sizeof(proj_path), "%s/export_test.r01proj", RETR01_FIXTURES_DIR);
    snprintf(cart_path, sizeof(cart_path), "%s/export_test.retr01", RETR01_FIXTURES_DIR);

    retr01_project_init_default(&proj, RETR01_PALETTE_V01_PATH);
    memset(proj.screens[0].screen.tiles, 1, RETR01_SCREEN_TILE_BYTES);
    retr01_attr_set(proj.screens[0].screen.attrs, 0, 0, 0);

    ASSERT_EQ(retr01_project_save(&proj, proj_path), 0);
    ASSERT_EQ(retr01_project_export_retr01(&proj, cart_path), 0);
    ASSERT_EQ(retr01_cart_load_file(cart_path, &cart), 0);
    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 0, 0, &loaded), 0);
    ASSERT_EQ(loaded.tiles[0], 1);

    retr01_cart_free(&cart);
}

TEST_RUNNER_BEGIN("test_project_io")
RUN_TEST_RC(project_save_load_roundtrip);
RUN_TEST_RC(project_export_cart);
TEST_RUNNER_END()
