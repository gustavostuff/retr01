#include "test_harness.h"

#include "retr01/rle.h"

#include <string.h>

TEST(roundtrip_randomish)
{
    uint8_t tiles[RETR01_SCREEN_TILE_BYTES];
    uint8_t attrs[RETR01_SCREEN_ATTR_BYTES];
    uint8_t out[RETR01_SCREEN_BYTES * 2];
    uint8_t t2[RETR01_SCREEN_TILE_BYTES];
    uint8_t a2[RETR01_SCREEN_ATTR_BYTES];
    size_t out_len = 0;
    size_t i;

    for (i = 0; i < sizeof(tiles); i++) {
        tiles[i] = (uint8_t)(i & 0xFF);
    }
    for (i = 0; i < sizeof(attrs); i++) {
        attrs[i] = (uint8_t)((i * 7) & 0xFF);
    }

    ASSERT_EQ(retr01_screen_rle_encode(tiles, attrs, out, sizeof(out), &out_len), 0);
    ASSERT(out_len > 0);
    ASSERT_EQ(retr01_screen_rle_decode(out, out_len, t2, a2), 0);
    ASSERT(memcmp(tiles, t2, sizeof(tiles)) == 0);
    ASSERT(memcmp(attrs, a2, sizeof(attrs)) == 0);
}

TEST(all_zero)
{
    uint8_t tiles[RETR01_SCREEN_TILE_BYTES];
    uint8_t attrs[RETR01_SCREEN_ATTR_BYTES];
    uint8_t out[512];
    uint8_t t2[RETR01_SCREEN_TILE_BYTES];
    uint8_t a2[RETR01_SCREEN_ATTR_BYTES];
    size_t out_len = 0;

    memset(tiles, 0, sizeof(tiles));
    memset(attrs, 0, sizeof(attrs));

    ASSERT_EQ(retr01_screen_rle_encode(tiles, attrs, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(retr01_screen_rle_decode(out, out_len, t2, a2), 0);
    ASSERT(memcmp(tiles, t2, sizeof(tiles)) == 0);
}

TEST(truncated_fails)
{
    uint8_t tiles[RETR01_SCREEN_TILE_BYTES];
    uint8_t attrs[RETR01_SCREEN_ATTR_BYTES];
    uint8_t out[512];
    uint8_t t2[RETR01_SCREEN_TILE_BYTES];
    uint8_t a2[RETR01_SCREEN_ATTR_BYTES];
    size_t out_len = 0;

    memset(tiles, 0, sizeof(tiles));
    memset(attrs, 0, sizeof(attrs));
    ASSERT_EQ(retr01_screen_rle_encode(tiles, attrs, out, sizeof(out), &out_len), 0);
    ASSERT(retr01_screen_rle_decode(out, out_len - 1, t2, a2) != 0);
}

TEST_RUNNER_BEGIN("test_rle")
RUN_TEST_RC(roundtrip_randomish);
RUN_TEST_RC(all_zero);
RUN_TEST_RC(truncated_fails);
TEST_RUNNER_END()
