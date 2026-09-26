#include "r01a_board.h"
#include "r01a_raster.h"
#include "r01_kit_palette.h"
#include "test_common.h"

#include "discrete_ic/video_sink.h"

static int rgb_match(const uint8_t *pix, uint8_t idx) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    r01_kit_rgb((int)idx, &r, &g, &b);
    return pix[0] == r && pix[1] == g && pix[2] == b;
}

static const uint8_t *pix_at(const uint8_t *rgb, int x, int y) {
    return rgb + ((size_t)y * (size_t)NS_VIDEO_W + (size_t)x) * 3u;
}

static int rgb_has_kit(const uint8_t *rgb, uint8_t idx) {
    int y;
    int x;
    for (y = 0; y < NS_VIDEO_H; y++) {
        for (x = 0; x < NS_VIDEO_W; x++) {
            if (rgb_match(pix_at(rgb, x, y), idx)) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    R01aBoard board;
    const uint8_t *rgb;

    r01a_board_init(&board);
    r01a_board_reset(&board);
    expect_true(r01a_field_kit_at(&board.field_sram, 84, 56) == 33, "field blit before scan");
    ns_video_sink_set_scale_2x(&board.sink, 1);
    r01a_board_step_dots(&board, (uint32_t)R01A_BEAM_DOTS_X * (uint32_t)R01A_BEAM_DOTS_Y);
    expect_true(r01a_ad724_encode_ok(&board.ad724), "AD724 encoding after a field");
    rgb = ns_video_sink_rgb(&board.sink);
    expect_true(rgb != NULL, "sink rgb");
    expect_true(rgb_match(pix_at(rgb, 16, 10), R01A_BAR_INDEX[0]), "BG1 bar");
    expect_true(rgb_has_kit(rgb, 33) || rgb_has_kit(rgb, 18), "S1 player visible (example_01 kits)");
    expect_true(rgb_has_kit(rgb, 14) || rgb_has_kit(rgb, 18), "S1 BG0 hills on screen");
    r01a_board_shutdown(&board);
    return test_done("test_tier_c_priority");
}
