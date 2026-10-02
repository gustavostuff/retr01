#include "r01a_board.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/bus.h"
#include "discrete_ic/video_sink.h"
#include "test_common.h"

static int sink_lit(const R01aBoard *board) {
    const uint8_t *rgb = ns_video_sink_rgb(&board->sink);
    size_t i;
    if (!rgb) {
        return 0;
    }
    for (i = 0; i < (size_t)NS_VIDEO_W * (size_t)NS_VIDEO_H * 3u; i++) {
        if (rgb[i] != 0) {
            return 1;
        }
    }
    return 0;
}

static int hole_on_tip(const NsBreadboard *bb, int wx, int wy, NsPbHole *out) {
    return ns_breadboard_tip_strip(bb, wx, wy, NULL) && ns_breadboard_hit_hole(bb, wx, wy, out);
}

static int place_manual_clock(R01aBoard *board, NsPbHole *clk_out_h, NsPbHole *clk_in_h) {
    int col;
    int clk_col;
    NsEntity *u04 = r01a_sn74hcu04_entity(&board->u04);
    NsEntity *bx = r01a_atf22v10_entity(&board->beam_x);

    r01a_board_jumper_clear(board);
    for (col = 2; col < NS_PB_COLS - 2; col++) {
        NsPbHole vcc_h = {col, NS_PB_LANE_E};
        NsPbHole vcc_found;
        NsPbHole y1_found;
        NsPbHole a1_found;
        NsPbHole gnd_found;
        NsPbHole rail;
        NsPbHole gnd_rail;
        int vtx, vty, ytx, yty, atx, aty, gtx, gty;
        int s_vcc, s_y1, s_a1, s_gnd, s_clk, s_rail, s_gnd_rail;

        if (!ns_breadboard_hole_exists(vcc_h)) {
            continue;
        }
        {
            int want = ns_breadboard_strip_id(vcc_h);
            int got;
            r01a_place_pin_on_hole(u04, 14, &board->breadboard, vcc_h);
            if (!ns_entity_pin_tip_board(u04, 14, &vtx, &vty) ||
                !ns_breadboard_tip_strip(&board->breadboard, vtx, vty, &got) || got != want) {
                continue;
            }
            if (!hole_on_tip(&board->breadboard, vtx, vty, &vcc_found)) {
                continue;
            }
        }
        if (!ns_entity_pin_tip_board(u04, 2, &ytx, &yty) ||
            !hole_on_tip(&board->breadboard, ytx, yty, &y1_found)) {
            continue;
        }
        if (!ns_entity_pin_tip_board(u04, 1, &atx, &aty) ||
            !hole_on_tip(&board->breadboard, atx, aty, &a1_found)) {
            continue;
        }
        if (!ns_entity_pin_tip_board(u04, 7, &gtx, &gty) ||
            !hole_on_tip(&board->breadboard, gtx, gty, &gnd_found)) {
            continue;
        }
        s_vcc = ns_breadboard_strip_id(vcc_found);
        s_y1 = ns_breadboard_strip_id(y1_found);
        s_a1 = ns_breadboard_strip_id(a1_found);
        s_gnd = ns_breadboard_strip_id(gnd_found);
        if (s_vcc == s_y1 || s_vcc == s_a1 || s_y1 == s_a1 || s_gnd == s_vcc || s_gnd == s_y1) {
            continue;
        }
        rail.col = vcc_found.col;
        rail.lane = NS_PB_LANE_TOP_POS;
        gnd_rail.col = gnd_found.col;
        gnd_rail.lane = NS_PB_LANE_TOP_NEG;
        if (!ns_breadboard_hole_exists(rail) || !ns_breadboard_hole_exists(gnd_rail)) {
            continue;
        }
        s_rail = ns_breadboard_strip_id(rail);
        s_gnd_rail = ns_breadboard_strip_id(gnd_rail);
        if (s_rail == s_vcc || s_rail == s_y1 || s_gnd_rail == s_gnd) {
            continue;
        }
        for (clk_col = 2; clk_col < NS_PB_COLS - 2; clk_col++) {
            NsPbHole clk_h = {clk_col, NS_PB_LANE_F};
            int ctx;
            int cty;

            if (!ns_breadboard_hole_exists(clk_h)) {
                continue;
            }
            r01a_place_pin_on_hole(bx, 1, &board->breadboard, clk_h);
            if (!ns_entity_pin_tip_board(bx, 1, &ctx, &cty) ||
                !hole_on_tip(&board->breadboard, ctx, cty, clk_in_h)) {
                continue;
            }
            s_clk = ns_breadboard_strip_id(*clk_in_h);
            if (s_y1 == s_clk || s_vcc == s_clk || s_a1 == s_clk) {
                continue;
            }
            r01a_board_jumper_clear(board);
            if (!r01a_board_jumper_add(board, rail, vcc_found)) {
                continue;
            }
            if (!r01a_board_jumper_add(board, gnd_rail, gnd_found)) {
                continue;
            }
            *clk_out_h = y1_found;
            return 1;
        }
    }
    return 0;
}

int main(void) {
    R01aBoard board;
    NsPbHole clk_out_h;
    NsPbHole clk_in_h = {40, NS_PB_LANE_F};
    int i;
    int toggles;
    NsLevel prev_clk;
    int x0;

    r01a_board_init(&board);
    r01a_board_step_dots(&board, (uint32_t)(NS_SCALE_1X_OY + 1) * (uint32_t)NS_RASTER_DOTS_X);
    expect_true(sink_lit(&board), "Auto burst lights the LCD");

    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    expect_true(!sink_lit(&board), "Manual toggle blanks the LCD");
    r01a_board_reset(&board);
    r01a_board_step_dots(&board, 64);
    expect_true(!sink_lit(&board), "Manual unwired: LCD blank");
    expect_true(ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y") == NS_LVL_Z,
                "Manual unwired: 1Y hi-Z");

    expect_true(place_manual_clock(&board, &clk_out_h, &clk_in_h), "place U04 VCC/GND/1Y on holes with rail jumpers");
    expect_true(r01a_board_jumper_add(&board, clk_out_h, clk_in_h), "1Y-CLK jumper");

    prev_clk = ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y");
    toggles = 0;
    x0 = r01a_atf22v10_x(&board.beam_x);
    for (i = 0; i < 16; i++) {
        r01a_board_step(&board);
        if (ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y") != prev_clk) {
            toggles++;
            prev_clk = ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y");
        }
    }
    expect_true(toggles >= 8, "Manual VCC/GND strips: 1Y toggles");
    expect_true(ns_entity_sense(r01a_atf22v10_entity(&board.beam_x), "CLK") ==
                    ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y"),
                "Manual jumper: CLK follows 1Y");
    expect_true(r01a_atf22v10_x(&board.beam_x) > x0, "Manual jumper: Beam X advances");

    r01a_board_shutdown(&board);
    return test_done("test_manual_bb");
}
