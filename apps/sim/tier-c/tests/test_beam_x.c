#include "atf22v10.h"

#include "netlist_sim/bus.h"
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
    expect_true(r01a_atf22v10_index(&pld) == 0, "Beam X does not own the index");
    expect_true(ns_entity_sense(e, "X5") == NS_LVL_L, "X=0 bit5 low");
    expect_true(ns_entity_pin_named(e, "INDEX0") == NULL, "no INDEX pins on Beam X");

    for (i = 0; i < 32; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 32, "32 falls -> X=32");
    expect_true(ns_entity_sense(e, "X5") == NS_LVL_H, "X=32 bit5 high");
    expect_true(ns_entity_sense(e, "X6") == NS_LVL_L, "X=32 bit6 low");

    for (i = 32; i < 256; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 256, "X reached 256");
    expect_true(r01a_atf22v10_hblank(&pld), "HBLANK at X=256");

    for (i = 256; i < R01A_BEAM_DOTS_X; i++) {
        clock_fall(e);
    }
    expect_true(r01a_atf22v10_x(&pld) == 0, "wrap 341 -> 0");
    expect_true(ns_entity_sense(e, "HWRAP") == NS_LVL_H, "HWRAP on wrap");

    return test_done("test_beam_x");
}
