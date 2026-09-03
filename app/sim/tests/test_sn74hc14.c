#include "retr01_sim/bus.h"
#include "sn74hc14.h"
#include "test_common.h"

int main(void) {
    R01sSn74hc14 chip;
    R01sEntity *e;

    r01s_sn74hc14_init(&chip, "U2");
    e = r01s_sn74hc14_entity(&chip);
    expect_true(e->pin_count == 14, "14 pins");

    r01s_entity_drive(e, "1A", R01S_LVL_H);
    r01s_entity_drive(e, "2A", R01S_LVL_L);
    r01s_entity_drive(e, "3A", R01S_LVL_H);
    r01s_entity_drive(e, "4A", R01S_LVL_L);
    r01s_entity_drive(e, "5A", R01S_LVL_H);
    r01s_entity_drive(e, "6A", R01S_LVL_L);
    r01s_entity_eval(e);

    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "1 invert");
    expect_true(r01s_entity_sense(e, "2Y") == R01S_LVL_H, "2 invert");
    expect_true(r01s_entity_sense(e, "3Y") == R01S_LVL_L, "3 invert");
    expect_true(r01s_entity_sense(e, "4Y") == R01S_LVL_H, "4 invert");
    expect_true(r01s_entity_sense(e, "5Y") == R01S_LVL_L, "5 invert");
    expect_true(r01s_entity_sense(e, "6Y") == R01S_LVL_H, "6 invert");

    /* Schmitt used as reset conditioner: active-low RES in -> active-low RESB out via two inverters */
    r01s_entity_drive(e, "1A", R01S_LVL_L); /* raw reset asserted */
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_H, "reset buffer stage1");

    return test_done("test_sn74hc14");
}
