#include "pads.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sPads chip;
    R01sEntity *e;

    r01s_pads_init(&chip, "PAD");
    e = r01s_pads_entity(&chip);
    expect_true(e->dip_pins == 14, "14-pin package");

    r01s_pads_set(&chip, 0, 0xA5);
    r01s_pads_set(&chip, 1, 0x5A);
    expect_true(r01s_pads_get(&chip, 0) == 0xA5, "port0");
    expect_true(r01s_pads_get(&chip, 1) == 0x5A, "port1");

    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "A0", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0xA5, "read FE60");

    r01s_entity_drive(e, "A0", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0x5A, "read FE61");

    r01s_entity_drive(e, "CE#", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "Hi-Z when CE# high");

    return test_done("test_pads");
}
