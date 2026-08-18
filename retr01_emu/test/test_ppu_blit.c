#include "../../retr01_world_studio/tests/test_harness.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/palette.h"
#include "retr01/screen.h"
#include "retr01_emu/ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fill_solid_tile(uint8_t *chr, int tile_index, uint8_t ci)
{
    uint8_t plane0 = 0;
    uint8_t plane1 = 0;

    if (ci & 1) {
        plane0 = 0xFF;
    }
    if (ci & 2) {
        plane1 = 0xFF;
    }

    for (int b = 0; b < 8; b++) {
        chr[(size_t)tile_index * 16 + (size_t)b] = plane0;
        chr[(size_t)tile_index * 16 + 8 + (size_t)b] = plane1;
    }
}

TEST(solid_tile_pixel)
{
    retr01_map_build_screen_t screens[1];
    retr01_map_build_world_t worlds[1];
    retr01_cart_t cart;
    retr01_screen_t decoded;
    retr01_ppu_t ppu;
    uint8_t rgba[RETR01_FB_W * RETR01_FB_H * 4];
    uint8_t *map = NULL;
    size_t map_len = 0;
    retr01_rgb_t expect;

    retr01_cart_init(&cart);
    retr01_screen_clear(&screens[0].screen);
    screens[0].screen.col = 0;
    screens[0].screen.row = 0;
    memset(screens[0].screen.tiles, 1, RETR01_SCREEN_TILE_BYTES);

    worlds[0].desc.grid_w = 1;
    worlds[0].desc.grid_h = 1;
    worlds[0].desc.empty_off = 0;
    worlds[0].screens = screens;
    worlds[0].screen_count = 1;

    ASSERT_EQ(retr01_map_build(worlds, 1, &map, &map_len), 0);

    cart.chr = (uint8_t *)calloc(512, 1);
    ASSERT(cart.chr != NULL);
    cart.chr_size = 512;
    fill_solid_tile(cart.chr, 1, 2);
    cart.map = map;
    cart.map_size = map_len;
    cart.world_count = 1;

    ASSERT_EQ(retr01_map_load_screen(&cart, 0, 0, 0, &decoded), 0);

    retr01_ppu_init(&ppu);
    ppu.screen = decoded;
    ppu.chr = cart.chr;
    ppu.chr_size = cart.chr_size;
    ASSERT_EQ(retr01_palette_load_v01(RETR01_PALETTE_V01_PATH, &ppu.palette), 0);

    retr01_ppu_render_bg(&ppu, rgba);

    expect = ppu.palette.entries[ppu.palette.bg_palettes[0][2]];
    ASSERT_EQ(rgba[(120 * RETR01_FB_W + 128) * 4 + 0], expect.r);
    ASSERT_EQ(rgba[(120 * RETR01_FB_W + 128) * 4 + 1], expect.g);
    ASSERT_EQ(rgba[(120 * RETR01_FB_W + 128) * 4 + 2], expect.b);

    free(cart.chr);
    free(map);
}

TEST_RUNNER_BEGIN("test_ppu_blit")
RUN_TEST_RC(solid_tile_pixel);
TEST_RUNNER_END()
