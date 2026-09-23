#include "r01a_board.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/bus.h"
#include "netlist_sim/passive.h"
#include "test_common.h"

#include <string.h>

int main(void) {
    R01aBoard board;
    NsPassive *r;
    NsPbHole r1_h = {2, NS_PB_LANE_E};
    int t2x;
    int t2y;
    int osc_tx;
    int osc_ty;
    int i;
    int toggles;
    NsLevel prev;

    r01a_board_init(&board);
    expect_true(board.passives.count == 21, "passive BOM size");
    r = NULL;
    for (i = 0; i < board.passives.count; i++) {
        if (board.passives.parts[i].kind == NS_PASSIVE_R &&
            strcmp(board.passives.parts[i].value, "33") == 0) {
            r = &board.passives.parts[i];
            break;
        }
    }
    expect_true(r != NULL, "33 ohm series R");

    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    {
        NsPbHole rail = {2, NS_PB_LANE_TOP_POS};
        NsPbHole term = {2, NS_PB_LANE_A};
        expect_true(r01a_board_jumper_add(&board, rail, term), "north rail to column 2");
    }
    ns_passive_set_orient(r, NS_ORIENT_0);
    {
        int hx;
        int hy;
        int tx;
        int ty;
        ns_breadboard_hole_world(&board.breadboard, r1_h, &hx, &hy);
        ns_passive_set_pivot(r, 0, 0);
        expect_true(ns_passive_tip_board(r, 1, &tx, &ty), "R pin1");
        ns_passive_set_pivot(r, hx - tx, hy - ty);
    }
    expect_true(ns_passive_tip_board(r, 2, &t2x, &t2y), "R pin2");
    ns_entity_place(r01a_osc_dot_entity(&board.osc_dot), 0, 0);
    expect_true(ns_entity_pin_tip_board(r01a_osc_dot_entity(&board.osc_dot), 14, &osc_tx, &osc_ty),
                "OSC VDD tip");
    ns_entity_place(r01a_osc_dot_entity(&board.osc_dot), t2x - osc_tx, t2y - osc_ty);

    prev = ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT");
    toggles = 0;
    for (i = 0; i < 16; i++) {
        r01a_board_step(&board);
        if (ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT") != prev) {
            toggles++;
            prev = ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "DOT");
        }
    }
    expect_true(ns_entity_sense(r01a_osc_dot_entity(&board.osc_dot), "VDD") == NS_LVL_H,
                "resistor carries VDD");
    expect_true(toggles >= 8, "33 ohm resistor conducts VDD in Manual");

    /* Caps occupy holes but do not pass DC. */
    {
        NsPassive *c = NULL;
        NsEntity *osc = r01a_osc_dot_entity(&board.osc_dot);
        int ctx;
        int cty;
        int ovx;
        int ovy;
        for (i = 0; i < board.passives.count; i++) {
            if (board.passives.parts[i].kind == NS_PASSIVE_CCAP) {
                c = &board.passives.parts[i];
                break;
            }
        }
        expect_true(c != NULL, "100 nF cap");
        ns_passive_set_pivot(r, -400, -400);
        ns_passive_set_orient(c, NS_ORIENT_0);
        ns_passive_set_pivot(c, 0, 0);
        expect_true(ns_passive_tip_board(c, 1, &ctx, &cty), "C pin1");
        {
            int hx;
            int hy;
            ns_breadboard_hole_world(&board.breadboard, r1_h, &hx, &hy);
            ns_passive_set_pivot(c, hx - ctx, hy - cty);
        }
        expect_true(ns_passive_tip_board(c, 2, &t2x, &t2y), "C pin2");
        ns_entity_place(osc, 0, 0);
        expect_true(ns_entity_pin_tip_board(osc, 14, &ovx, &ovy), "OSC VDD tip 2");
        ns_entity_place(osc, t2x - ovx, t2y - ovy);
        r01a_board_step(&board);
        expect_true(ns_entity_sense(osc, "VDD") != NS_LVL_H, "cap does not pass DC");
    }

    {
        NsBreadboard *bb2;
        NsEntity *osc = r01a_osc_dot_entity(&board.osc_dot);
        NsPbHole rail = {2, NS_PB_LANE_TOP_POS};
        NsPbHole dest = {10, NS_PB_LANE_A};
        NsPbHole vddh = {10, NS_PB_LANE_E};
        int hx;
        int hy;
        int tx;
        int ty;
        bb2 = r01a_board_add_breadboard(&board, 400, 40);
        expect_true(bb2 != NULL, "BB2");
        expect_true(r01a_board_jumper_add_across(&board, &board.breadboard, rail, bb2, dest, 200, 40, 40),
                    "inter-bb jumper");
        ns_breadboard_hole_world(bb2, vddh, &hx, &hy);
        ns_entity_place(osc, 0, 0);
        expect_true(ns_entity_pin_tip_board(osc, 14, &tx, &ty), "OSC VDD tip 3");
        ns_entity_place(osc, hx - tx, hy - ty);
        r01a_board_step(&board);
        expect_true(ns_entity_sense(osc, "VDD") == NS_LVL_H, "inter-bb jumper carries VDD");
    }

    r01a_board_shutdown(&board);
    return test_done("test_passives");
}
