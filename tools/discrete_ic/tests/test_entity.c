#include "discrete_ic/entity.h"

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
    expect_true(e.body_w == 19 * NS_PX_PER_MM, "horizontal body_w");
    expect_true(e.body_h == ns_dip_snap_across_px(6 * NS_PX_PER_MM), "horizontal body_h snapped");
    expect_true(ns_dip_body_along_px(40) == 52 * NS_PX_PER_MM, "DIP-40 length px");
    expect_true(e.body_h == 15, "DIP-14 across -> 15 px (tip span 20)");
    expect_true(NS_PX_PER_MM == 2, "2 px per mm");
    expect_true(NS_DIP_PIN_PITCH_PX == 5, "pin pitch 5");
    expect_true(e.health == NS_HEALTH_BOOT, "health boot default");

    ns_entity_set_orient(&e, NS_ORIENT_V);
    expect_true(e.body_w == 15 && e.body_h == 19 * NS_PX_PER_MM, "vertical swap snapped");

    ns_entity_add_pin(&e, 1, "A", NS_PIN_IN);
    expect_true(ns_entity_pin(&e, 1) != NULL, "pin 1");
    expect_true(ns_entity_pin(&e, 99) == NULL, "missing pin");

    if (g_fail) {
        fprintf(stderr, "test_ns_entity: FAILED\n");
        return 1;
    }
    printf("test_ns_entity: ok\n");
    return 0;
}
