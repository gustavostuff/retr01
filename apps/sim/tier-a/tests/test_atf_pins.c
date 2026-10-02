#include "atf22v10.h"
#include "test_common.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/entity.h"

static int hole_y(const NsBreadboard *bb, NsPbHole h) {
    int wx;
    int wy;
    ns_breadboard_hole_world(bb, h, &wx, &wy);
    (void)wx;
    return wy;
}

static void place_pin_on_hole(NsEntity *e, int pin, const NsBreadboard *bb, NsPbHole h) {
    int hx;
    int hy;
    int tx;
    int ty;
    ns_breadboard_hole_world(bb, h, &hx, &hy);
    ns_entity_place(e, 0, 0);
    expect_true(ns_entity_pin_tip_board(e, pin, &tx, &ty), "tip before place");
    ns_entity_place(e, hx - tx, hy - ty);
}

int main(void) {
    R01aAtf22v10 pld;
    NsBreadboard bb;
    NsEntity *e;
    int tx;
    int ty;
    int i;
    int col = 10;
    NsPbHole hg = {col, NS_PB_LANE_F + 1}; /* lane G */
    NsPbHole he = {col, NS_PB_LANE_E};
    int strips[24];
    int nstrip = 0;

    r01a_atf22v10_init(&pld, "UPLDX", R01A_PLD_BEAM_X);
    ns_breadboard_init(&bb, "BB1");
    e = r01a_atf22v10_entity(&pld);
    r01a_test_pitch_ic(e);
    expect_true(ns_entity_dip_row_span_px(e) == NS_PB_PITCH * 2 + NS_PB_GAP_TRENCH, "ATF row span on tier-A bb");

    /* Straddle the trench: pin 1 on G, pin 13 on E. Opposite pins must not share a strip. */
    place_pin_on_hole(e, 1, &bb, hg);
    expect_true(ns_entity_pin_tip_board(e, 1, &tx, &ty) && ty == hole_y(&bb, hg), "pin 1 on G");
    expect_true(ns_entity_pin_tip_board(e, 13, &tx, &ty) && ty == hole_y(&bb, he), "pin 13 on E");
    for (i = 1; i <= 24; i++) {
        int s;
        expect_true(ns_entity_pin_tip_board(e, i, &tx, &ty), "ATF pin tip");
        expect_true(ns_breadboard_tip_strip(&bb, tx, ty, &s), "ATF pin on a hole");
        strips[nstrip++] = s;
    }
    for (i = 0; i < nstrip; i++) {
        int j;
        for (j = i + 1; j < nstrip; j++) {
            expect_true(strips[i] != strips[j], "straddle: no two ATF pins share a strip");
        }
    }

    /* Same 20 px span on one half (pin 1 on E, pin 24 on A) shorts the column. */
    place_pin_on_hole(e, 1, &bb, he);
    {
        int s1 = -1;
        int s24 = -1;
        expect_true(ns_entity_pin_tip_board(e, 1, &tx, &ty) && ty == hole_y(&bb, he), "same-side pin 1 on E");
        expect_true(ns_entity_pin_tip_board(e, 24, &tx, &ty) && ty == hole_y(&bb, (NsPbHole){col, NS_PB_LANE_A}),
                    "same-side pin 24 on A");
        expect_true(ns_entity_pin_tip_board(e, 1, &tx, &ty) && ns_breadboard_tip_strip(&bb, tx, ty, &s1),
                    "pin 1 strip");
        expect_true(ns_entity_pin_tip_board(e, 24, &tx, &ty) && ns_breadboard_tip_strip(&bb, tx, ty, &s24),
                    "pin 24 strip");
        expect_true(s1 == s24 && s1 >= 0, "same-side seating shorts opposite pins on one column");
    }

    return test_done("test_atf_pins");
}
