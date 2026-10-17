#include "discrete_ic/breadboard.h"
#include "discrete_ic/entity.h"
#include "discrete_ic/pin_header.h"

#include <stdio.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    NsEntity e;
    NsEntityVTable vt = {NULL, NULL, NULL, NULL};

    ns_entity_init(&e, &vt, "DIP14", "U1");
    ns_entity_set_dip(&e, 14);
    expect_true(e.dip_pins == 14, "14-pin package");
    expect_true(e.orient == NS_ORIENT_H, "default horizontal");
    expect_true(e.pkg_len_mm == 19 && e.pkg_wid_mm == 6, "DIP-14 mm outline");
#ifdef R01A_BB_3PX
    {
        int span = NS_PB_PITCH + NS_PB_GAP_TRENCH;
        int pitch = NS_DIP_PIN_PITCH_PX;
        int row = 6 * pitch;
        int along = row + NS_PB_PITCH;
        expect_true(e.body_w == along, "horizontal body_w on 3px lattice");
        expect_true(e.body_h == span, "horizontal body_h = row span");
        expect_true(ns_dip_body_along_px(40) == 52 * NS_PX_PER_MM, "DIP-40 length px");
        expect_true(NS_PX_PER_MM == 2, "2 px per mm");
        expect_true(NS_DIP_PIN_PITCH_PX == 5, "pin pitch 5");
        expect_true(e.health == NS_HEALTH_BOOT, "health boot default");

        ns_entity_set_orient(&e, NS_ORIENT_V);
        expect_true(e.body_w == span && e.body_h == along, "vertical swap on 3px lattice");
    }
#else
    expect_true(e.body_w == 19 * NS_PX_PER_MM, "horizontal body_w");
    expect_true(e.body_h == ns_dip_snap_across_px(6 * NS_PX_PER_MM), "horizontal body_h snapped");
    expect_true(ns_dip_body_along_px(40) == 52 * NS_PX_PER_MM, "DIP-40 length px");
    expect_true(e.body_h == 15, "DIP-14 across -> 15 px (tip span 20)");
    expect_true(NS_PX_PER_MM == 2, "2 px per mm");
    expect_true(NS_DIP_PIN_PITCH_PX == 5, "pin pitch 5");
    expect_true(e.health == NS_HEALTH_BOOT, "health boot default");

    ns_entity_set_orient(&e, NS_ORIENT_V);
    expect_true(e.body_w == 15 && e.body_h == 19 * NS_PX_PER_MM, "vertical swap snapped");
#endif

    ns_entity_add_pin(&e, 1, "A", NS_PIN_IN);
    expect_true(ns_entity_pin(&e, 1) != NULL, "pin 1");
    expect_true(ns_entity_pin(&e, 99) == NULL, "missing pin");

    ns_entity_init(&e, &vt, "ATF22V10", "UPLDX");
    ns_entity_set_dip_mm(&e, 24, 32, 8);
#ifdef R01A_BB_3PX
    {
        int span = NS_PB_PITCH * 2 + NS_PB_GAP_TRENCH;
        expect_true(ns_entity_dip_row_span_px(&e) == span, "ATF22 row span on tier-A bb");
        ns_entity_place(&e, 0, 0);
        {
            int tx;
            int ty1;
            int ty13;
            expect_true(ns_entity_pin_tip_board(&e, 1, &tx, &ty1), "ATF22 pin 1 tip");
            expect_true(ns_entity_pin_tip_board(&e, 13, &tx, &ty13), "ATF22 pin 13 tip");
            expect_true((ty1 > ty13 ? ty1 - ty13 : ty13 - ty1) == span, "ATF22 opposing tips tier-A bb span");
        }
    }
#else
    expect_true(ns_entity_dip_row_span_px(&e) == 20, "ATF22 row span 20 px (E/G + extra pitch)");
    ns_entity_place(&e, 0, 0);
    {
        int tx;
        int ty1;
        int ty13;
        expect_true(ns_entity_pin_tip_board(&e, 1, &tx, &ty1), "ATF22 pin 1 tip");
        expect_true(ns_entity_pin_tip_board(&e, 13, &tx, &ty13), "ATF22 pin 13 tip");
        expect_true(ty1 - ty13 == 20, "ATF22 opposing tips 20 px");
    }
#endif

    ns_entity_init(&e, &vt, "AT27C256R", "U24");
    ns_entity_set_dip_mm(&e, 28, 37, 14);
#ifdef R01A_BB_3PX
    expect_true(e.body_w == 37 * NS_PX_PER_MM && e.body_h == 30,
                "28P6 body px (across = row span on 3px lattice)");
#else
    expect_true(e.body_w == 37 * NS_PX_PER_MM && e.body_h == 14 * NS_PX_PER_MM - NS_DIP_WIDE_BODY_TRIM_PX,
                "28P6 exact body px (plastic trimmed for pin rows)");
#endif
    expect_true(ns_entity_dip_row_span_px(&e) == 30, "28P6 row span 30 px (600 mil)");
    ns_entity_add_pin(&e, 1, "VPP", NS_PIN_PWR);
    ns_entity_add_pin(&e, 28, "VCC", NS_PIN_PWR);
#ifdef R01A_BB_3PX
    e.pkg_pitch_px = 6;
    ns_entity_refresh_body(&e);
    expect_true(e.body_w == 84 && e.body_h == 30, "28P6 body after pitch 6");
#endif
    ns_entity_place(&e, 0, 0);
    {
        int tx1, ty1, tx28, ty28;
        expect_true(ns_entity_pin_tip_board(&e, 1, &tx1, &ty1), "pin 1 tip");
        expect_true(ns_entity_pin_tip_board(&e, 28, &tx28, &ty28), "pin 28 tip");
        expect_true(ty28 - ty1 == 30 || ty1 - ty28 == 30, "opposing pin tips 30 px apart (E/I rows)");
    }

    ns_entity_init(&e, &vt, "HDR", "J2");
    ns_entity_set_pin_header(&e, 6, 1);
    expect_true(e.visual == NS_ENTITY_VIS_PIN_HDR, "pin header visual");
    expect_true(e.body_w == 6 * NS_PIN_HDR_CELL_PX && e.body_h == NS_PIN_HDR_CELL_PX, "1x6 body px");
    ns_entity_add_pin(&e, 1, "R", NS_PIN_IN);
    ns_entity_place(&e, 10, 20);
    {
        int tx;
        int ty;
        expect_true(ns_entity_pin_tip_board(&e, 1, &tx, &ty), "pin 1 tip");
        expect_true(tx == 10 + NS_PIN_HDR_CELL_PX / 2, "pin 1 tip x");
        expect_true(ty == 20 + NS_PIN_HDR_CELL_PX / 2, "pin 1 tip y");
    }

    if (g_fail) {
        fprintf(stderr, "test_ns_entity: FAILED\n");
        return 1;
    }
    printf("test_ns_entity: ok\n");
    return 0;
}
