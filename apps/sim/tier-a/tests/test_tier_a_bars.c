#include "r01a_board.h"
#include "r01a_raster.h"
#include "r01_kit_palette.h"
#include "test_common.h"

#include "netlist_sim/video_sink.h"

static int rgb_match(const uint8_t *pix, uint8_t idx) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    r01_kit_rgb((int)idx, &r, &g, &b);
    return pix[0] == r && pix[1] == g && pix[2] == b;
}

int main(void) {
    R01aBoard board;
    const uint8_t *rgb;
    int bi;
    static const int sample_x[8] = {16, 48, 80, 112, 144, 176, 208, 240};

    r01a_board_init(&board);
    r01a_board_step_dots(&board, (uint32_t)R01A_BEAM_DOTS_X * (uint32_t)R01A_BEAM_DOTS_Y);
    expect_true(r01a_ad724_encode_ok(&board.ad724), "AD724 encoding after a field");
    rgb = ns_video_sink_rgb(&board.sink);
    expect_true(rgb != NULL, "sink rgb");
    for (bi = 0; bi < 8; bi++) {
        const uint8_t *pix = rgb + ((size_t)10 * (size_t)NS_VIDEO_W + (size_t)sample_x[bi]) * 3u;
        expect_true(rgb_match(pix, R01A_BAR_INDEX[bi]), "bar pixel matches kit index");
    }
    r01a_board_shutdown(&board);
    return test_done("test_tier_a_bars");
}
