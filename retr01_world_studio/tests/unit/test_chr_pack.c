#include "retr01/chr_pack.h"

#include "../test_harness.h"

#include <string.h>

TEST(test_ci8x8_to_chr_solid)
{
    uint8_t ci[64];
    uint8_t tile[16];
    int i;

    for (i = 0; i < 64; i++) {
        ci[i] = 2;
    }
    retr01_ci8x8_to_chr(ci, tile);
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(tile[i], 0);
        ASSERT_EQ(tile[8 + i], 0xFF);
    }
}

TEST(test_pack_canvas_uniform)
{
    uint8_t ci[256 * 240];
    uint8_t chr[256 * 16];
    retr01_screen_t screen;
    int unique = 0;

    memset(ci, 1, sizeof(ci));
    memset(chr, 0, sizeof(chr));
    ASSERT_EQ(retr01_pack_canvas(ci, 256, 240, 0, chr, sizeof(chr), &screen, &unique), 0);
    ASSERT_EQ(unique, 1);
    ASSERT_EQ(screen.tiles[0], 0);
    ASSERT_EQ(screen.tiles[RETR01_SCREEN_TILE_BYTES - 1], 0);
    ASSERT_EQ(retr01_attr_get(screen.attrs, 0, 0), 0);
}

TEST(test_pack_canvas_two_tiles)
{
    uint8_t ci[256 * 240];
    uint8_t chr[256 * 16];
    retr01_screen_t screen;
    int unique = 0;
    int x;
    int y;

    memset(ci, 0, sizeof(ci));
    for (y = 0; y < 240; y++) {
        for (x = 0; x < 128; x++) {
            ci[y * 256 + x] = 1;
        }
    }

    ASSERT_EQ(retr01_pack_canvas(ci, 256, 240, 1, chr, sizeof(chr), &screen, &unique), 0);
    ASSERT_EQ(unique, 2);
    ASSERT_EQ(screen.tiles[0], 0);
    ASSERT_EQ(screen.tiles[16], 1);
    ASSERT_EQ(retr01_attr_get(screen.attrs, 0, 0), 1);
}

TEST(test_count_unique_tiles)
{
    uint8_t tiles[RETR01_SCREEN_TILE_BYTES];
    int count = 0;

    memset(tiles, 0, sizeof(tiles));
    tiles[0] = 0;
    tiles[1] = 1;
    tiles[2] = 1;
    tiles[3] = 2;
    ASSERT_EQ(retr01_count_unique_tiles(tiles, &count), 0);
    ASSERT_EQ(count, 3);
}

TEST(test_pack_clears_sprite_page)
{
    uint8_t ci[256 * 240];
    uint8_t chr[512 * 16];
    retr01_screen_t screen;
    int unique = 0;
    size_t i;

    memset(ci, 2, sizeof(ci));
    memset(chr, 0xAA, sizeof(chr));
    ASSERT_EQ(retr01_pack_canvas(ci, 256, 240, 0, chr, sizeof(chr), &screen, &unique), 0);
    ASSERT_EQ(unique, 1);
    for (i = 256u * 16u; i < sizeof(chr); i++) {
        ASSERT_EQ(chr[i], 0);
    }
}

TEST(test_pack_two_canvases_share_chr)
{
    static uint8_t a[256 * 240];
    static uint8_t b[256 * 240];
    uint8_t chr[512 * 16];
    const uint8_t *planes[2];
    uint8_t pals[2] = {0, 1};
    retr01_screen_t screens[2];
    int unique = 0;

    memset(a, 0, sizeof(a));
    memset(b, 1, sizeof(b));
    memset(screens, 0, sizeof(screens));
    screens[0].col = 0;
    screens[1].col = 1;
    planes[0] = a;
    planes[1] = b;

    ASSERT_EQ(retr01_pack_canvases(planes, pals, 2, chr, sizeof(chr), screens, &unique), 0);
    ASSERT_EQ(unique, 2);
    ASSERT_EQ(screens[0].col, 0);
    ASSERT_EQ(screens[1].col, 1);
    ASSERT_EQ(screens[0].tiles[0], 0);
    ASSERT_EQ(screens[1].tiles[0], 1);
    ASSERT_EQ(retr01_attr_get(screens[1].attrs, 0, 0), 1);
}

TEST_RUNNER_BEGIN("test_chr_pack")
RUN_TEST_RC(test_ci8x8_to_chr_solid);
RUN_TEST_RC(test_pack_canvas_uniform);
RUN_TEST_RC(test_pack_canvas_two_tiles);
RUN_TEST_RC(test_count_unique_tiles);
RUN_TEST_RC(test_pack_clears_sprite_page);
RUN_TEST_RC(test_pack_two_canvases_share_chr);
TEST_RUNNER_END()
