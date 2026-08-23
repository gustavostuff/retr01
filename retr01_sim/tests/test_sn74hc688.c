#include "retr01_sim/bus.h"
#include "sn74hc688.h"
#include "test_common.h"

#include <stdio.h>

static void drive_byte(R01sEntity *e, const char *prefix, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

int main(void) {
    R01sSn74hc688 chip;
    R01sEntity *e;

    r01s_sn74hc688_init(&chip, "U41");
    e = r01s_sn74hc688_entity(&chip);
    expect_true(e->pin_count == 20, "20 pins");

    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    drive_byte(e, "P", 0x64);
    drive_byte(e, "Q", 0x64);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "EQ#") == R01S_LVL_L, "equal low");

    drive_byte(e, "P", 0x65);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "EQ#") == R01S_LVL_H, "unequal high");

    drive_byte(e, "P", 0x64);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "EQ#") == R01S_LVL_H, "OE masks");

    return test_done("test_sn74hc688");
}
