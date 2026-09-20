#include "r01a_board.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/bus.h"
#include "netlist_sim/video_sink.h"
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

static void place_pin_on_hole(NsEntity *e, int pin, const NsBreadboard *bb, NsPbHole h) {
    int hx;
    int hy;
    int tx;
    int ty;
    ns_entity_place(e, 0, 0);
    expect_true(ns_entity_pin_tip_board(e, pin, &tx, &ty), "pin tip");
    ns_breadboard_hole_world(bb, h, &hx, &hy);
    ns_entity_place(e, hx - tx, hy - ty);
}

static int hole_on_tip(const NsBreadboard *bb, int wx, int wy, NsPbHole *out) {
    NsPbHole h;
    int hx;
    int hy;
    if (!ns_breadboard_hit_hole(bb, wx, wy, &h)) {
        return 0;
    }
    ns_breadboard_hole_world(bb, h, &hx, &hy);
    if (hx != wx || hy != wy) {
        return 0;
    }
    if (out) {
        *out = h;
    }
    return 1;
}

static int place_manual_clock(R01aBoard *board, NsPbHole *dot_h, NsPbHole *clk_h) {
    int col;
    int lane;
    NsEntity *osc = r01a_osc_dot_entity(&board->osc_dot);
    NsEntity *pwr = r01a_pwr5v_entity(&board->pwr);
    NsEntity *bx = r01a_atf22v10_entity(&board->beam_x);

    for (lane = NS_PB_LANE_A; lane <= NS_PB_LANE_J; lane++) {
        for (col = 0; col < NS_PB_COLS; col++) {
            NsPbHole vdd_h = {col, lane};
            NsPbHole vdd_found;
            NsPbHole dot_found;
            NsPbHole pwr_h;
            int dtx;
            int dty;
            int vtx;
            int vty;
            if (!ns_breadboard_hole_exists(vdd_h)) {
                continue;
            }
            place_pin_on_hole(osc, 8, &board->breadboard, vdd_h);
            if (!ns_entity_pin_tip_board(osc, 8, &vtx, &vty) || !hole_on_tip(&board->breadboard, vtx, vty, &vdd_found)) {
                continue;
            }
            if (!ns_entity_pin_tip_board(osc, 5, &dtx, &dty) || !hole_on_tip(&board->breadboard, dtx, dty, &dot_found)) {
                continue;
            }
            pwr_h.col = vdd_found.col;
            pwr_h.lane = (vdd_found.lane <= NS_PB_LANE_E) ? NS_PB_LANE_A : NS_PB_LANE_F;
            if (!ns_breadboard_hole_exists(pwr_h)) {
                pwr_h = vdd_found;
            }
            place_pin_on_hole(pwr, 3, &board->breadboard, pwr_h);
            place_pin_on_hole(bx, 1, &board->breadboard, *clk_h);
            if (ns_breadboard_strip_id(dot_found) == ns_breadboard_strip_id(*clk_h)) {
                continue;
            }
            *dot_h = dot_found;
            return 1;
        }
    }
    return 0;
}

int main(void) {
    R01aBoard board;
    NsPbHole dot_h;
    NsPbHole clk_h = {40, NS_PB_LANE_A};
    int i;
    int toggles;
    NsLevel prev_dot;
    int x0;

    r01a_board_init(&board);
    r01a_board_step_dots(&board, 64);
    expect_true(r01a_ad724_encode_ok(&board.ad724), "Auto encode after burst");
    expect_true(sink_lit(&board), "Auto burst lights the LCD");

    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    expect_true(!sink_lit(&board), "Manual toggle blanks the LCD");
    expect_true(!r01a_ad724_encode_ok(&board.ad724), "Manual toggle drops encode");
    r01a_board_reset(&board);
    r01a_board_step_dots(&board, 64);
    expect_true(!r01a_ad724_encode_ok(&board.ad724), "Manual unwired: no encode");
    expect_true(ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT") == NS_LVL_Z,
                "Manual unwired: DOT hi-Z");

    expect_true(place_manual_clock(&board, &dot_h, &clk_h), "place OSC VDD/DOT and Beam X CLK on holes");
    expect_true(r01a_board_jumper_add(&board, dot_h, clk_h), "DOT-CLK jumper");

    prev_dot = ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT");
    toggles = 0;
    x0 = r01a_atf22v10_x(&board.beam_x);
    for (i = 0; i < 16; i++) {
        r01a_board_step(&board);
        if (ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT") != prev_dot) {
            toggles++;
            prev_dot = ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT");
        }
    }
    expect_true(toggles >= 8, "Manual VDD strip: DOT toggles");
    expect_true(ns_entity_sense(r01a_atf22v10_entity(&board.beam_x), "CLK") ==
                    ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT"),
                "Manual jumper: CLK follows DOT");
    expect_true(r01a_atf22v10_x(&board.beam_x) > x0, "Manual jumper: Beam X advances");

    r01a_board_shutdown(&board);
    return test_done("test_manual_bb");
}
