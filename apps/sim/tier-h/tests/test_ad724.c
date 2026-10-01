#include "ad724.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sAd724 chip;
    R01sEntity *e;

    r01s_ad724_init(&chip, "U725");
    e = r01s_ad724_entity(&chip);
    expect_true(e->pin_count == 16, "16 pins");

    r01s_entity_drive(e, "APOS", R01S_LVL_H);
    r01s_entity_drive(e, "DPOS", R01S_LVL_H);
    r01s_entity_drive(e, "AGND", R01S_LVL_L);
    r01s_entity_drive(e, "DGND", R01S_LVL_L);
    r01s_entity_drive(e, "ENCD", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "COMP") == R01S_LVL_Z, "COMP Hi-Z when ENCD low");

    r01s_entity_drive(e, "ENCD", R01S_LVL_H);
    r01s_entity_drive(e, "RIN", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "COMP") == R01S_LVL_H, "COMP high when RGB present");

    return test_done("test_ad724");
}
