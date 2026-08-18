#include "test_harness.h"

#include "retr01/palette.h"

TEST(load_v01)
{
    retr01_master_palette_t pal;

    ASSERT_EQ(retr01_palette_load_v01(RETR01_PALETTE_V01_PATH, &pal), 0);
    ASSERT_EQ(pal.entries[0].r, 0x00);
    ASSERT_EQ(pal.entries[0].g, 0x00);
    ASSERT_EQ(pal.entries[0].b, 0x00);
    ASSERT_EQ(pal.entries[48].r, 0xFF);
    ASSERT_EQ(pal.entries[48].g, 0xFF);
    ASSERT_EQ(pal.entries[48].b, 0xFF);
    ASSERT_EQ(pal.bg_palettes[0][0], 0);
    ASSERT_EQ(pal.bg_palettes[0][1], 20);
    ASSERT_EQ(pal.bg_palettes[0][3], 52);
    ASSERT_EQ(pal.bg_palettes[1][1], 23);
}

TEST_RUNNER_BEGIN("test_palette")
RUN_TEST_RC(load_v01);
TEST_RUNNER_END()
