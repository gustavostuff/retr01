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
    NsEntity *bx = r01a_atf22v10_entity(&board->beam_x);

    r01a_board_jumper_clear(board);
    for (lane = NS_PB_LANE_A; lane <= NS_PB_LANE_J; lane++) {
        for (col = 0; col < NS_PB_COLS; col++) {
            NsPbHole vdd_h = {col, lane};
            NsPbHole vdd_found;
            NsPbHole dot_found;
            NsPbHole oe_found;
            NsPbHole gnd_found;
            NsPbHole rail;
            int vtx;
            int vty;
            int dtx;
            int dty;
            int oex;
            int oey;
            int gnx;
            int gny;
            int s_vdd;
            int s_dot;
            int s_oe;
            int s_gnd;
            int s_clk;
            int s_rail;
            if (!ns_breadboard_hole_exists(vdd_h)) {
                continue;
            }
            place_pin_on_hole(osc, 14, &board->breadboard, vdd_h);
            if (!ns_entity_pin_tip_board(osc, 14, &vtx, &vty) ||
                !hole_on_tip(&board->breadboard, vtx, vty, &vdd_found)) {
                continue;
            }
            if (!ns_entity_pin_tip_board(osc, 8, &dtx, &dty) ||
                !hole_on_tip(&board->breadboard, dtx, dty, &dot_found)) {
                continue;
            }
            if (!ns_entity_pin_tip_board(osc, 1, &oex, &oey) ||
                !hole_on_tip(&board->breadboard, oex, oey, &oe_found)) {
                continue;
            }
            if (!ns_entity_pin_tip_board(osc, 7, &gnx, &gny) ||
                !hole_on_tip(&board->breadboard, gnx, gny, &gnd_found)) {
                continue;
            }
            s_vdd = ns_breadboard_strip_id(vdd_found);
            s_dot = ns_breadboard_strip_id(dot_found);
            s_oe = ns_breadboard_strip_id(oe_found);
            s_gnd = ns_breadboard_strip_id(gnd_found);
            if (s_vdd == s_dot || s_vdd == s_oe || s_vdd == s_gnd || s_dot == s_oe || s_dot == s_gnd ||
                s_oe == s_gnd) {
                continue;
            }
            rail.col = vdd_found.col;
            rail.lane = NS_PB_LANE_TOP_POS;
            if (!ns_breadboard_hole_exists(rail)) {
                continue;
            }
            s_rail = ns_breadboard_strip_id(rail);
            if (s_rail == s_vdd || s_rail == s_dot || s_rail == s_oe || s_rail == s_gnd) {
                continue;
            }
            r01a_board_jumper_clear(board);
            if (!r01a_board_jumper_add(board, rail, vdd_found)) {
                continue;
            }
            place_pin_on_hole(bx, 1, &board->breadboard, *clk_h);
            s_clk = ns_breadboard_strip_id(*clk_h);
            if (s_dot == s_clk || s_vdd == s_clk || s_gnd == s_clk) {
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
    NsPbHole clk_h = {40, NS_PB_LANE_F};
    int i;
    int toggles;
    NsLevel prev_dot;
    int x0;

    r01a_board_init(&board);
    r01a_board_step_dots(&board, (uint32_t)(NS_SCALE_1X_OY + 1) * (uint32_t)NS_RASTER_DOTS_X);
    expect_true(sink_lit(&board), "Auto burst lights the LCD");

    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    expect_true(!sink_lit(&board), "Manual toggle blanks the LCD");
    r01a_board_reset(&board);
    r01a_board_step_dots(&board, 64);
    expect_true(!sink_lit(&board), "Manual unwired: LCD blank");
    expect_true(ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT") == NS_LVL_Z,
                "Manual unwired: DOT hi-Z");

    expect_true(place_manual_clock(&board, &dot_h, &clk_h), "place OSC VDD/DOT on holes with north-rail jumper");
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
