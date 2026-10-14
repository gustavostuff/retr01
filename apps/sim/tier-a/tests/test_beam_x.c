#include "atf22v10.h"

#include "discrete_ic/bus.h"
#include "r01a_raster.h"
#include "test_common.h"

static void clock_fall(NsEntity *e) {
    ns_entity_drive(e, "CLK", NS_LVL_H);
    ns_entity_tick(e);
    ns_entity_drive(e, "CLK", NS_LVL_L);
    ns_entity_tick(e);
}

int main(void) {
    R01aAtf22v10 pld;
    NsEntity *e;
    int i;

    r01a_atf22v10_init(&pld, "UPLDX", R01A_PLD_BEAM_X);
    e = r01a_atf22v10_entity(&pld);
    ns_entity_drive(e, "VCC", NS_LVL_H);
    ns_entity_drive(e, "RES#", NS_LVL_H);
    ns_entity_eval(e);
    expect_true(r01a_atf22v10_x(&pld) == 0, "reset X is 0");
    expect_true(ns_entity_sense(e, "HSYNC") == NS_LVL_L, "HSYNC low at X=0");
    expect_true(ns_entity_sense(e, "Q0") == NS_LVL_L, "Q0 low at X=0");
    expect_true(ns_entity_pin_named(e, "INDEX0") == NULL, "no INDEX pins");
    expect_true(ns_entity_pin_named(e, "HWRAP") == NULL, "no HWRAP pin");
    expect_true(ns_entity_pin_named(e, "CSYNC") == NULL, "no CSYNC pin");

    clock_fall(e);
    expect_true(r01a_atf22v10_x(&pld) == 1, "one fall -> X=1");
    expect_true(ns_entity_sense(e, "Q0") == NS_LVL_H, "Q0 is the low bit");

    for (i = 1; i < 16; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 16, "16 falls -> X=16");

    for (i = 16; i < 256; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 256, "X reached 256");
    expect_true(r01a_atf22v10_hblank(&pld), "past visible width at X=256");
    expect_true(ns_entity_sense(e, "HSYNC") == NS_LVL_L, "HSYNC still low at X=256");

    for (i = 256; i < R01A_HSYNC_START; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == R01A_HSYNC_START, "X at HSYNC start");
    expect_true(ns_entity_sense(e, "HSYNC") == NS_LVL_H, "HSYNC high in the sync window");

    for (i = R01A_HSYNC_START; i < R01A_BEAM_DOTS_X; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 0, "wrap 341 -> 0");
    expect_true(ns_entity_sense(e, "HSYNC") == NS_LVL_L, "HSYNC low after wrap");

    return test_done("test_beam_x");
}
