#include "atf22v10.h"

#include "discrete_ic/bus.h"
#include "r01a_raster.h"
#include "test_common.h"

static void clock_rise(NsEntity *e) {
    ns_entity_drive(e, "CLK", NS_LVL_L);
    ns_entity_tick(e);
    ns_entity_drive(e, "CLK", NS_LVL_H);
    ns_entity_tick(e);
}

int main(void) {
    R01aAtf22v10 pld;
    NsEntity *e;
    int i;

    r01a_atf22v10_init(&pld, "UPLDY", R01A_PLD_BEAM_Y);
    e = r01a_atf22v10_entity(&pld);
    ns_entity_drive(e, "VCC", NS_LVL_H);
    ns_entity_drive(e, "RES#", NS_LVL_H);
    ns_entity_eval(e);
    expect_true(r01a_atf22v10_y(&pld) == 0, "reset Y is 0");

    for (i = 0; i < R01A_BEAM_VISIBLE_H; i++) {
        clock_rise(e);
    }
    expect_true(r01a_atf22v10_y(&pld) == R01A_BEAM_VISIBLE_H, "Y reached 240");
    expect_true(r01a_atf22v10_vblank(&pld), "VBLANK at Y=240");

    for (i = R01A_BEAM_VISIBLE_H; i < R01A_BEAM_DOTS_Y; i++) {
        clock_rise(e);
    }
    expect_true(r01a_atf22v10_y(&pld) == 0, "wrap 262 -> 0");

    return test_done("test_beam_y");
}
