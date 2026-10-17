#include "ad724.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sAd724 chip;
    R01sEntity *e;

    r01s_ad724_init(&chip, "U2075");
    e = r01s_ad724_entity(&chip);
    expect_true(e->pin_count == 11, "wired pins");

    r01s_entity_drive(e, "VCC1", R01S_LVL_L);
    r01s_entity_drive(e, "GND1", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "CVOUT") == R01S_LVL_Z, "CVOUT Hi-Z when VCC1 is low");

    r01s_entity_drive(e, "VCC1", R01S_LVL_H);
    r01s_entity_drive(e, "RIN", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "CVOUT") == R01S_LVL_H, "CVOUT high when RGB present");

    return test_done("test_ad724");
}
