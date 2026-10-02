#include "r01a_board.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/bus.h"
#include "discrete_ic/passive.h"
#include "discrete_ic/ui_passive_assets.h"
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
    expect_true(board.passives.count == 22, "passive BOM size");
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
        NsPbHole r2_h = {r1_h.col + 4, r1_h.lane};
        int t2x;
        int t2y;
        r01a_seat_passive_on_holes(r, &board.breadboard, r1_h, r2_h);
        expect_true(ns_passive_tip_board(r, 2, &t2x, &t2y), "R pin2 after seat");
        expect_true(ns_breadboard_tip_strip(&board.breadboard, t2x, t2y, NULL), "R pin2 on hole");
    }
    expect_true(ns_passive_tip_board(r, 2, &t2x, &t2y), "R pin2");
    ns_entity_place(r01a_sn74hcu04_entity(&board.u04), 0, 0);
    expect_true(ns_entity_pin_tip_board(r01a_sn74hcu04_entity(&board.u04), 14, &osc_tx, &osc_ty),
                "U04 VCC tip");
    ns_entity_place(r01a_sn74hcu04_entity(&board.u04), t2x - osc_tx, t2y - osc_ty);

    prev = ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y");
    toggles = 0;
    for (i = 0; i < 16; i++) {
        r01a_board_step(&board);
        if (ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y") != prev) {
            toggles++;
            prev = ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "1Y");
        }
    }
    expect_true(ns_entity_sense(r01a_sn74hcu04_entity(&board.u04), "VCC") == NS_LVL_H,
                "resistor carries VCC");
    expect_true(toggles >= 8, "33 ohm resistor conducts VCC in Manual");

    /* Caps occupy holes but do not pass DC. */
    {
        NsPassive *c = NULL;
        NsEntity *u04_ent = r01a_sn74hcu04_entity(&board.u04);
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
        ns_entity_place(u04_ent, 0, 0);
        expect_true(ns_entity_pin_tip_board(u04_ent, 14, &ovx, &ovy), "U04 VCC tip 2");
        ns_entity_place(u04_ent, t2x - ovx, t2y - ovy);
        r01a_board_step(&board);
        expect_true(ns_entity_sense(u04_ent, "VCC") != NS_LVL_H, "cap does not pass DC");
    }

    {
        NsBreadboard *bb2;
        NsEntity *u04_ent = r01a_sn74hcu04_entity(&board.u04);
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
        ns_entity_place(u04_ent, 0, 0);
        expect_true(ns_entity_pin_tip_board(u04_ent, 14, &tx, &ty), "U04 VCC tip 3");
        ns_entity_place(u04_ent, hx - tx, hy - ty);
        r01a_board_step(&board);
        expect_true(ns_entity_sense(u04_ent, "VCC") == NS_LVL_H, "inter-bb jumper carries VCC");
    }

    {
        R01aBoard gndb;
        NsBreadboard *bb2;
        NsEntity *u04_ent;
        NsEntity *prom;
        NsPbHole rail = {2, NS_PB_LANE_TOP_NEG};
        NsPbHole dest = {10, NS_PB_LANE_A};
        NsPbHole gndh = {10, NS_PB_LANE_E};
        NsPbHole gndh2 = {10, NS_PB_LANE_F};
        int hx;
        int hy;
        int tx;
        int ty;
        r01a_board_init(&gndb);
        r01a_board_set_wire_mode(&gndb, R01A_WIRE_MANUAL);
        u04_ent = r01a_sn74hcu04_entity(&gndb.u04);
        prom = r01a_at27c256r_entity(&gndb.prom);
        bb2 = r01a_board_add_breadboard(&gndb, 400, 40);
        expect_true(bb2 != NULL, "BB2 gnd");
        expect_true(r01a_board_jumper_add_across(&gndb, &gndb.breadboard, rail, bb2, dest, 40, 40, 40),
                    "inter-bb gnd jumper");
        ns_breadboard_hole_world(bb2, gndh, &hx, &hy);
        ns_entity_place(u04_ent, 0, 0);
        expect_true(ns_entity_pin_tip_board(u04_ent, 7, &tx, &ty), "U04 GND tip");
        ns_entity_place(u04_ent, hx - tx, hy - ty);
        ns_breadboard_hole_world(bb2, gndh2, &hx, &hy);
        ns_entity_place(prom, 0, 0);
        expect_true(ns_entity_pin_tip_board(prom, 14, &tx, &ty), "PROM GND tip");
        ns_entity_place(prom, hx - tx, hy - ty);
        expect_true(r01a_board_jumper_add_on(&gndb, bb2, gndh, gndh2, 40, 40, 40), "tie GND strips");
        r01a_board_step(&gndb);
        expect_true(ns_entity_sense(u04_ent, "GND") == NS_LVL_L, "inter-bb jumper carries GND");
        expect_true(ns_entity_sense(prom, "GND") == NS_LVL_L, "second GND pin on bridged net");
        expect_true(ns_entity_sense(u04_ent, "GND") != NS_LVL_X, "tied GND pins are not a bus fight");
        r01a_board_shutdown(&gndb);
    }

    {
        R01aBoard loadb;
        NsPassive *r75 = NULL;
        NsEntity *u04_ent;
        NsPbHole gnd = {0, NS_PB_LANE_TOP_NEG};
        NsPbHole loadh = {10, NS_PB_LANE_A};
        int hx;
        int hy;
        int tx;
        int ty;
        r01a_board_init(&loadb);
        r01a_board_set_wire_mode(&loadb, R01A_WIRE_MANUAL);
        u04_ent = r01a_sn74hcu04_entity(&loadb.u04);
        for (i = 0; i < loadb.passives.count; i++) {
            if (loadb.passives.parts[i].kind == NS_PASSIVE_R &&
                strcmp(loadb.passives.parts[i].value, "75.0") == 0) {
                r75 = &loadb.passives.parts[i];
                break;
            }
        }
        expect_true(r75 != NULL, "75 ohm load R");
        expect_true(r01a_board_jumper_add(&loadb, gnd, loadh), "GND to load column");
        ns_passive_set_orient(r75, NS_ORIENT_0);
        ns_passive_set_pivot(r75, 0, 0);
        expect_true(ns_passive_tip_board(r75, 2, &tx, &ty), "75 pin2");
        ns_breadboard_hole_world(&loadb.breadboard, loadh, &hx, &hy);
        ns_passive_set_pivot(r75, hx - tx, hy - ty);
        ns_entity_place(u04_ent, 0, 0);
        expect_true(ns_entity_pin_tip_board(u04_ent, 7, &tx, &ty), "U04 GND for load");
        ns_breadboard_hole_world(&loadb.breadboard, gnd, &hx, &hy);
        ns_entity_place(u04_ent, hx - tx, hy - ty);
        r01a_board_step(&loadb);
        expect_true(ns_entity_sense(u04_ent, "GND") == NS_LVL_L, "75 ohm to GND does not bus-fight GND");
        r01a_board_shutdown(&loadb);
    }

    {
        NsPassive a;
        NsPassive b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        a.kind = NS_PASSIVE_R;
        b.kind = NS_PASSIVE_R;
        ns_passive_set_orient(&a, NS_ORIENT_0);
        ns_passive_set_orient(&b, NS_ORIENT_0);
        ns_passive_set_pivot(&a, 10, 10);
        ns_passive_set_pivot(&b, 10, 15);
        expect_true(ns_passive_hit(&a, 10, 10), "R A pivot hit");
        expect_true(!ns_passive_hit(&b, 10, 10), "R B misses A pivot");
        expect_true(ns_passive_hit(&b, 10, 15), "R B pivot hit");
        expect_true(!ns_passive_hit(&a, 10, 15), "R A misses B pivot");
        ns_passive_set_orient(&a, NS_ORIENT_90);
        ns_passive_set_pivot(&a, 40, 40);
        expect_true(ns_passive_hit(&a, 40, 40), "R rotated pivot hit");
        expect_true(!ns_passive_hit(&a, 40 - 5, 40), "R rotated misses neighbor pitch");
        ns_passive_set_orient(&a, NS_ORIENT_0);
        ns_passive_set_pivot(&a, 0, 0);
        ns_passive_set_leg_ext(&a, 2, 10);
        {
            int tx = 0;
            int ty = 0;
#ifdef R01A_BB_3PX
            int tip2 = 24 + 10;
#else
            int tip2 = 20 + 10;
#endif
            expect_true(ns_passive_tip_board(&a, 2, &tx, &ty), "R stretch pin2 tip");
            expect_true(tx == tip2 && ty == 0, "R stretch pin2 at span+extra");
            expect_true(ns_passive_hit(&a, tip2, 0), "R stretch lead hit");
        }
        ns_passive_set_leg_ext(&a, 1, 5);
        {
            int tx = 0;
            int ty = 0;
            expect_true(ns_passive_tip_board(&a, 1, &tx, &ty), "R stretch pin1 tip");
            expect_true(tx == -5 && ty == 0, "R stretch pin1 opposite axis");
        }
    }

    r01a_board_shutdown(&board);
    return test_done("test_passives");
}
