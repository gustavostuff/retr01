#include "retr01_sim/bus.h"
#include "sst39sf040.h"
#include "test_common.h"

#include <stdio.h>

static void drive_addr(R01sEntity *e, uint32_t addr) {
    static const char *const names[19] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6",  "A7",  "A8",
                                          "A9",  "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17",
                                          "A18"};
    int i;
    for (i = 0; i < 19; i++) {
        r01s_entity_drive(e, names[i], (addr & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void set_ctrl(R01sEntity *e, int ce, int oe, int we) {
    r01s_entity_drive(e, "CE#", ce ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(e, "OE#", oe ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(e, "WE#", we ? R01S_LVL_L : R01S_LVL_H);
}

int main(void) {
    R01sSst39sf040 chip;
    R01sEntity *e;
    uint8_t patch[] = {0x12, 0x34, 0x56};

    r01s_sst39sf040_init(&chip, "U40");
    e = r01s_sst39sf040_entity(&chip);
    expect_true(e->pin_count == 32, "32 pins");
    expect_true(e->dip_pins == 32, "32-pin DIP");
    expect_true(r01s_sst39sf040_peek(&chip, 0) == 0xFF, "erased FF");

    r01s_sst39sf040_load(&chip, 0x10000, patch, 3);
    expect_true(r01s_sst39sf040_peek(&chip, 0x10000) == 0x12, "load mid");
    expect_true(r01s_sst39sf040_peek(&chip, 0x10002) == 0x56, "load end");

    /* Standby */
    set_ctrl(e, 0, 1, 0);
    drive_addr(e, 0x10000);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "CE# Hi-Z");

    /* Read */
    set_ctrl(e, 1, 1, 0);
    drive_addr(e, 0x10000);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0x12, "read DQ");

    drive_addr(e, 0x10001);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0x34, "read next");

    /* High address bit A18 */
    r01s_sst39sf040_poke(&chip, 0x40000, 0xAB);
    drive_addr(e, 0x40000);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0xAB, "A18 path");

    /* WE# asserted blocks read drive */
    set_ctrl(e, 1, 1, 1);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "WE# blocks read");

    return test_done("test_sst39sf040");
}
