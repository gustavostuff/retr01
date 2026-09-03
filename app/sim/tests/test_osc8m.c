#include "osc8m.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sOsc8m chip;
    R01sEntity *e;
    R01sLevel a, b;

    r01s_osc8m_init(&chip, "Y1");
    e = r01s_osc8m_entity(&chip);
    expect_true(e->dip_pins == 8, "8-pin package");
    expect_true(e->pin_count == 4, "4 modeled pins");

    r01s_entity_drive(e, "VDD", R01S_LVL_L);
    r01s_entity_tick(e);
    expect_true(r01s_entity_sense(e, "PHI2") == R01S_LVL_Z, "no clock without VDD");

    r01s_entity_drive(e, "VDD", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_tick(e);
    a = r01s_entity_sense(e, "PHI2");
    r01s_entity_tick(e);
    b = r01s_entity_sense(e, "PHI2");
    expect_true(a != b, "PHI2 toggles");
    expect_true(a == R01S_LVL_H || a == R01S_LVL_L, "PHI2 digital");

    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_tick(e);
    expect_true(r01s_entity_sense(e, "PHI2") == R01S_LVL_Z, "OE# disables");

    return test_done("test_osc8m");
}
