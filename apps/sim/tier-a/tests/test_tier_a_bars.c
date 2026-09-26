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

static int rgb_black(const uint8_t *pix) {
    return pix[0] == 0 && pix[1] == 0 && pix[2] == 0;
}

static const uint8_t *pix_at(const uint8_t *rgb, int x, int y) {
    return rgb + ((size_t)y * (size_t)NS_VIDEO_W + (size_t)x) * 3u;
}

int main(void) {
    R01aBoard board;
    const uint8_t *rgb;
    int bi;
    int lw;
    int lh;
    static const int sample_x[8] = {16, 48, 80, 112, 144, 176, 208, 240};

    r01a_board_init(&board);
    expect_true(ns_video_sink_scale_2x(&board.sink) == 0, "LCD default 1x");
    ns_video_sink_lcd_size(&board.sink, &lw, &lh);
    expect_true(lw == NS_VIDEO_W && lh == NS_VIDEO_H, "LCD glyph is CRT field");

    r01a_board_step_dots(&board, (uint32_t)R01A_BEAM_DOTS_X * (uint32_t)R01A_BEAM_DOTS_Y);
    expect_true(r01a_ad724_encode_ok(&board.ad724), "AD724 encoding after a field");
    rgb = ns_video_sink_rgb(&board.sink);
    expect_true(rgb != NULL, "sink rgb");
    expect_true(rgb_black(pix_at(rgb, 0, 0)), "1x top-left overscan");
    expect_true(rgb_black(pix_at(rgb, sample_x[0], 10)), "1x top overscan");
    expect_true(rgb_black(pix_at(rgb, NS_VIDEO_W - 1, NS_VIDEO_H - 1)), "1x bottom-right overscan");
    for (bi = 0; bi < 8; bi++) {
        int x = sample_x[bi];
        int y = NS_SCALE_1X_OY + 10;
        if (x < NS_SCALE_1X_OX || x >= NS_SCALE_1X_OX + NS_LOGICAL_W) {
            expect_true(rgb_black(pix_at(rgb, x, y)), "1x side overscan");
        } else {
            expect_true(rgb_match(pix_at(rgb, x, y), R01A_BAR_INDEX[bi]), "1x playfield bar");
        }
    }

    ns_video_sink_set_scale_2x(&board.sink, 1);
    r01a_board_step_dots(&board, (uint32_t)R01A_BEAM_DOTS_X * (uint32_t)R01A_BEAM_DOTS_Y);
    rgb = ns_video_sink_rgb(&board.sink);
    for (bi = 0; bi < 8; bi++) {
        expect_true(rgb_match(pix_at(rgb, sample_x[bi], 10), R01A_BAR_INDEX[bi]),
                    "2x bar pixel matches kit index");
    }
    r01a_board_shutdown(&board);
    return test_done("test_tier_a_bars");
}
