#include "pwr5v.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sPwr5v chip;
    R01sEntity *e;

    r01s_pwr5v_init(&chip, "PS1");
    e = r01s_pwr5v_entity(&chip);
    expect_true(e->pin_count == 4, "4 pins");

    r01s_entity_drive(e, "VIN", R01S_LVL_L);
    r01s_entity_drive(e, "EN", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(chip.power_ok == 0, "no power without VIN");
    expect_true(r01s_entity_sense(e, "VDD") == R01S_LVL_L, "VDD low");

    r01s_entity_drive(e, "VIN", R01S_LVL_H);
    r01s_entity_drive(e, "EN", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(chip.power_ok == 0, "EN low disables");

    r01s_entity_drive(e, "EN", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(chip.power_ok == 1, "power ok");
    expect_true(r01s_entity_sense(e, "VDD") == R01S_LVL_H, "VDD high");

    r01s_entity_drive(e, "EN", R01S_LVL_Z);
    r01s_entity_eval(e);
    expect_true(chip.power_ok == 1, "EN floating enables");

    return test_done("test_pwr5v");
}
