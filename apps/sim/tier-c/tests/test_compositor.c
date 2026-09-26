#include "atf22v10.h"

#include "as6c62256.h"
#include "discrete_ic/bus.h"
#include "r01a_raster.h"
#include "test_common.h"

static void clock_rise(NsEntity *e) {
    ns_entity_drive(e, "CLK", NS_LVL_L);
    ns_entity_tick(e);
    ns_entity_drive(e, "CLK", NS_LVL_H);
    ns_entity_tick(e);
}

static void drive_cell(NsEntity *e, int xb, int yb, int hblank, int vblank) {
    ns_entity_drive(e, "X5", (xb & 1) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "X6", (xb & 2) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "X7", (xb & 4) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "Y5", (yb & 1) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "Y6", (yb & 2) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "Y7", (yb & 4) ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "HBLANK", hblank ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "VBLANK", vblank ? NS_LVL_H : NS_LVL_L);
}

int main(void) {
    R01aAtf22v10 pld;
    R01aAs6c62256 field;
    NsEntity *e;

    r01a_atf22v10_init(&pld, "UPLDC", R01A_PLD_COMPOSITOR);
    e = r01a_atf22v10_entity(&pld);
    ns_entity_drive(e, "VCC", NS_LVL_H);
    ns_entity_drive(e, "RES#", NS_LVL_H);
    ns_entity_drive(e, "PHI2", NS_LVL_L);
    ns_entity_drive(e, "RWB", NS_LVL_H);
    r01a_atf22v10_set_beam_dot(&pld, -1, -1);
    drive_cell(e, 0, 0, 0, 0);
    clock_rise(e);
    expect_true(r01a_atf22v10_index(&pld) == 48, "BG1 bar 0");

    drive_cell(e, 2, 1, 0, 0);
    r01a_atf22v10_set_beam_dot(&pld, 80, 10);
    clock_rise(e);
    expect_true(r01a_atf22v10_index(&pld) == R01A_BAND_INDEX[1], "BG1 hole falls through to band without BG0 ping");

    r01a_as6c62256_init(&field, "U41");
    r01a_as6c62256_poke(&field, (uint32_t)73 * 128u + 101u, 52);
    r01a_atf22v10_bind_field(&pld, field.mem, 0);
    expect_true(r01a_compositor_index(101, 73, 3, 2, 0, 0, field.mem, 0) == 52, "raster sprite over bar");
    drive_cell(e, 3, 2, 0, 0);
    r01a_atf22v10_set_beam_dot(&pld, 101, 73);
    clock_rise(e);
    expect_true(r01a_atf22v10_index(&pld) == 52, "field sprite wins over bars");

    drive_cell(e, 3, 2, 1, 0);
    clock_rise(e);
    expect_true(r01a_atf22v10_index(&pld) == 0, "HBLANK forces index 0");

    return test_done("test_compositor");
}
