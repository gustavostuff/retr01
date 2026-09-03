#include "retr01_sim/bus.h"
#include "sn74hc245.h"
#include "test_common.h"

#include <stdio.h>

static void drive_side(R01sEntity *e, char side, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%c%d", side, i + 1);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static uint8_t sense_side(R01sEntity *e, char side) {
    int i;
    uint8_t v = 0;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%c%d", side, i + 1);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

int main(void) {
    R01sSn74hc245 chip;
    R01sEntity *e;

    r01s_sn74hc245_init(&chip, "U20");
    e = r01s_sn74hc245_entity(&chip);
    expect_true(e->pin_count == 20, "20 pins");

    /* OE=L, DIR=H, A=$55 => B=$55 */
    r01s_entity_drive(e, "OE", R01S_LVL_L);
    r01s_entity_drive(e, "DIR", R01S_LVL_H);
    drive_side(e, 'A', 0x55);
    r01s_entity_eval(e);
    expect_true(sense_side(e, 'B') == 0x55, "A->B");

    /* Reverse: DIR=L, B=$AA => A=$AA */
    r01s_entity_drive(e, "DIR", R01S_LVL_L);
    drive_side(e, 'B', 0xAA);
    r01s_entity_eval(e);
    expect_true(sense_side(e, 'A') == 0xAA, "B->A");

    /* OE high => neither side driven by chip */
    r01s_entity_drive(e, "OE", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "A1") == R01S_LVL_Z, "OE Hi-Z A");
    expect_true(r01s_entity_sense(e, "B1") == R01S_LVL_Z, "OE Hi-Z B");

    return test_done("test_sn74hc245");
}
