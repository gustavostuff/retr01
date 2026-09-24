#include "r01a_board.h"
#include "r01a_netlist.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/entity.h"
#include "test_common.h"

#include "netlist_sim/island.h"

#include <string.h>

/* Stock Tier B boots seated. This test builds its own placement. */
static void r01a_test_unseat(R01aBoard *b) {
    NsIsland *island = ns_island_group_at_mut(r01a_board_group(b), 0);
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        NsEntity *e = island->entities[i];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            ns_passive_set_pivot((NsPassive *)e, -2000, -2000 - i * 30);
        } else {
            ns_entity_place(e, -2000, -2000 - i * 30);
        }
    }
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

static int has_kind(const R01aNetIssue *iss, int n, int kind) {
    int i;
    for (i = 0; i < n; i++) {
        if (iss[i].kind == kind) {
            return 1;
        }
    }
    return 0;
}

static int has_text_kind(const R01aNetIssue *iss, int n, int kind, const char *needle) {
    int i;
    for (i = 0; i < n; i++) {
        if (iss[i].kind == kind && strstr(iss[i].text, needle)) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    R01aBoard board;
    R01aNetIssue iss[R01A_NET_ISSUE_MAX];
    int n;
    NsEntity *osc;
    NsEntity *bx;
    NsPbHole vdd_h = {4, NS_PB_LANE_A};
    NsPbHole gnd_rail = {0, NS_PB_LANE_TOP_NEG};
    NsPbHole pos_rail = {0, NS_PB_LANE_TOP_POS};
    NsPbHole idx_h = {20, NS_PB_LANE_A};
    NsPbHole a0_h = {20, NS_PB_LANE_F};

    r01a_board_init(&board);
    r01a_test_unseat(&board);
    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
    expect_true(n > 0, "unwired Manual reports opens");

    osc = r01a_osc_dot_entity(&board.osc_dot);
    place_pin_on_hole(osc, 14, &board.breadboard, vdd_h);
    expect_true(r01a_board_jumper_add(&board, vdd_h, gnd_rail), "jumper VDD strip to GND rail");
    n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
    expect_true(has_kind(iss, n, R01A_NET_SHORT_PWR_GND), "5V tied to GND rail is a short");

    r01a_board_jumper_clear(&board);
    expect_true(r01a_board_jumper_add(&board, vdd_h, pos_rail), "jumper VDD to 5V rail");
    n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
    expect_true(!has_kind(iss, n, R01A_NET_SHORT_PWR_GND), "5V to +rail is not a 5V-GND short");

    bx = r01a_atf22v10_entity(&board.compositor);
    place_pin_on_hole(bx, 14, &board.breadboard, idx_h);
    expect_true(r01a_board_jumper_add(&board, idx_h, gnd_rail), "INDEX0 to GND");
    n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
    expect_true(has_kind(iss, n, R01A_NET_SHORT_DATA_GND), "INDEX0 on GND rail is data-GND short");
    {
        int pi;
        int marked = 0;
        for (pi = 0; pi < bx->pin_count; pi++) {
            if (bx->pins[pi].number == 14 && r01a_netlist_pin_shorted(bx, pi)) {
                marked = 1;
            }
        }
        expect_true(marked, "shorted pin is marked for the red pulse");
    }

    r01a_board_jumper_clear(&board);
    expect_true(r01a_board_jumper_add(&board, vdd_h, pos_rail), "restore 5V jumper");
    {
        NsEntity *prom = r01a_at27c256r_entity(&board.prom);
        int i;
        int open_index = 0;
        place_pin_on_hole(prom, 10, &board.breadboard, a0_h);
        n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
        for (i = 0; i < n; i++) {
            if (iss[i].kind == R01A_NET_MISSING && strstr(iss[i].text, "INDEX0")) {
                open_index = 1;
            }
        }
        expect_true(open_index, "INDEX0 and A0 on split strips is open");
        expect_true(r01a_board_jumper_add(&board, idx_h, a0_h), "INDEX0 to A0 jumper");
        n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
        open_index = 0;
        for (i = 0; i < n; i++) {
            if (iss[i].kind == R01A_NET_MISSING && strstr(iss[i].text, "INDEX0") &&
                strstr(iss[i].text, "A0")) {
                open_index = 1;
            }
        }
        expect_true(!open_index, "INDEX0-A0 jumper closes that net");
    }

    r01a_board_shutdown(&board);
    r01a_board_init(&board);
    r01a_test_unseat(&board);
    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    osc = r01a_osc_dot_entity(&board.osc_dot);
    {
        NsPbHole vdd_term = {4, NS_PB_LANE_A};
        NsPbHole pos_right = {40, NS_PB_LANE_TOP_POS};
        place_pin_on_hole(osc, 14, &board.breadboard, vdd_term);
        expect_true(ns_breadboard_hole_exists(pos_right), "right + rail hole");
        expect_true(r01a_board_jumper_add(&board, vdd_term, pos_right), "VDD to right + rail");
        n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
        expect_true(!has_text_kind(iss, n, R01A_NET_MISSING, "Y2.VDD"), "right-half + rail feeds Y2.VDD");
    }

    r01a_board_jumper_clear(&board);
    {
        NsPbHole vdd_term = {4, NS_PB_LANE_A};
        NsPbHole pos_south = {2, NS_PB_LANE_BOT_POS};
        place_pin_on_hole(osc, 14, &board.breadboard, vdd_term);
        expect_true(r01a_board_jumper_add(&board, vdd_term, pos_south), "VDD to south + rail");
        n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
        expect_true(!has_text_kind(iss, n, R01A_NET_MISSING, "Y2.VDD"), "south + rail feeds Y2.VDD");
    }

    r01a_board_shutdown(&board);
    r01a_board_init(&board);
    r01a_test_unseat(&board);
    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    {
        NsBreadboard *bb2;
        NsEntity *prom = r01a_at27c256r_entity(&board.prom);
        NsPbHole bb1_gnd = {50, NS_PB_LANE_TOP_NEG};
        NsPbHole bb2_gnd_r = {50, NS_PB_LANE_TOP_NEG};
        NsPbHole bb2_gnd_l = {2, NS_PB_LANE_TOP_NEG};
        NsPbHole prom_h = {5, NS_PB_LANE_F};
        bb2 = r01a_board_add_breadboard(&board, 400, 40);
        expect_true(bb2 != NULL, "BB2 for rail bridge");
        expect_true(ns_breadboard_hole_exists(bb1_gnd) && ns_breadboard_hole_exists(bb2_gnd_l),
                    "GND rail holes");
        expect_true(r01a_board_jumper_add_across(&board, &board.breadboard, bb1_gnd, bb2, bb2_gnd_r, 40, 40, 40),
                    "BB2 right GND to BB1");
        place_pin_on_hole(prom, 14, bb2, prom_h);
        expect_true(r01a_board_jumper_add_on(&board, bb2, prom_h, bb2_gnd_l, 40, 40, 40),
                    "PROM GND to BB2 left - rail");
        n = r01a_netlist_check(&board, iss, R01A_NET_ISSUE_MAX);
        expect_true(!has_text_kind(iss, n, R01A_NET_MISSING, "U24.GND"),
                    "BB2 left GND joins BB1 after a rail jumper");
    }

    r01a_board_shutdown(&board);
    return test_done("test_netlist");
}
