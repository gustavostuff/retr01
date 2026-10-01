#include "at27c256r.h"
#include "test_common.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/entity.h"

static int lane_y(int lane) {
    int y = NS_PB_MARGIN + lane * NS_PB_PITCH;
    if (lane > NS_PB_LANE_TOP_NEG) {
        y += NS_PB_GAP_RAIL;
    }
    if (lane > NS_PB_LANE_E) {
        y += NS_PB_GAP_TRENCH;
    }
    if (lane > NS_PB_LANE_J) {
        y += NS_PB_GAP_RAIL;
    }
    return y;
}

int main(void) {
    R01aAt27c256r prom;
    NsBreadboard bb;
    NsEntity *e;
    int hx;
    int hy;
    int tx;
    int ty;
    int i;
    int col = 10;
    NsPbHole h1 = {col, NS_PB_LANE_E};
    int t1x;
    int t1y;
    int t14x;
    int t14y;
    int t28x;
    int t28y;
    int t15x;
    int t15y;
    int row_span;

    r01a_at27c256r_init(&prom, "U24");
    ns_breadboard_init(&bb, "BB1");
    e = r01a_at27c256r_entity(&prom);
    row_span = ns_entity_dip_row_span_px(e);
    expect_true(row_span == 30, "28P6 row span 30 px");
    expect_true(e->body_w == 37 * NS_PX_PER_MM && e->body_h == 14 * NS_PX_PER_MM - NS_DIP_WIDE_BODY_TRIM_PX,
                "28P6 body px (plastic trimmed for pin rows)");

    ns_breadboard_hole_world(&bb, h1, &hx, &hy);
    ns_entity_place(e, 0, 0);
    expect_true(ns_entity_pin_tip_board(e, 1, &tx, &ty), "pin 1 tip");
    ns_entity_place(e, hx - tx, hy - ty);

    for (i = 1; i <= 28; i++) {
        expect_true(ns_entity_pin_tip_board(e, i, &tx, &ty), "pin tip");
        expect_true(ns_breadboard_hit_hole(&bb, tx, ty, NULL), "pin on breadboard lattice");
    }

    ns_entity_pin_tip_board(e, 1, &t1x, &t1y);
    ns_entity_pin_tip_board(e, 14, &t14x, &t14y);
    ns_entity_pin_tip_board(e, 28, &t28x, &t28y);
    ns_entity_pin_tip_board(e, 15, &t15x, &t15y);

    expect_true(t1x == t28x, "pin 1 and 28 same column (notch end)");
    expect_true(t14x == t15x, "pin 14 and 15 same column (far end)");
    expect_true(t14x - t1x == 13 * NS_DIP_PIN_PITCH_PX, "14 pins along row pitch");
    expect_true(t28y - t1y == row_span, "600 mil row spacing");
    expect_true(t1y == lane_y(NS_PB_LANE_E), "pin 1 on lane E when seated");
    expect_true(t28y == lane_y(10), "pin 28 on lane I (600 mil pair)");

    return test_done("test_at27_pins");
}
