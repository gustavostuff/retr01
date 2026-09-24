#include "as6c62256.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

static void set_ctrl(R01sEntity *e, int ce, int oe, int we) {
    r01s_entity_drive(e, "CE#", ce ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(e, "OE#", oe ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(e, "WE#", we ? R01S_LVL_L : R01S_LVL_H);
}

int main(void) {
    R01sAs6c62256 chip;
    R01sEntity *e;

    r01s_as6c62256_init(&chip, "U3");
    e = r01s_as6c62256_entity(&chip);
    expect_true(e->pin_count == 28, "28 pins");

    /* Standby: CE# high => DQ Hi-Z */
    set_ctrl(e, 0, 1, 0);
    r01s_bus_write(e, "A", 15, 0x1234);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "standby Hi-Z");

    /* Write $AA at $0100 */
    set_ctrl(e, 1, 0, 1);
    r01s_bus_write(e, "A", 15, 0x0100);
    r01s_bus_write(e, "DQ", 8, 0xAA);
    r01s_entity_eval(e);
    expect_true(r01s_as6c62256_peek(&chip, 0x0100) == 0xAA, "write mem");

    /* Read back */
    set_ctrl(e, 1, 1, 0);
    r01s_bus_write(e, "A", 15, 0x0100);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0xAA, "read DQ");

    /* Output disable */
    set_ctrl(e, 1, 0, 0);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "OE# high Hi-Z");

    return test_done("test_as6c62256");
}
