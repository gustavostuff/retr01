#include "retr01_sim/bus.h"
#include "sn74hc573.h"
#include "test_common.h"

#include <stdio.h>

static void drive_d(R01sEntity *e, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dD", i + 1);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static uint8_t sense_q(R01sEntity *e) {
    int i;
    uint8_t v = 0;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dQ", i + 1);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

int main(void) {
    R01sSn74hc573 chip;
    R01sEntity *e;

    r01s_sn74hc573_init(&chip, "U10");
    e = r01s_sn74hc573_entity(&chip);
    expect_true(e->pin_count == 20, "20 pins");
    expect_true(e->dip_pins == 20, "20-pin DIP");

    /* Transparent: OE=L, LE=H, D=$A5 => Q=$A5 */
    r01s_entity_drive(e, "OE", R01S_LVL_L);
    r01s_entity_drive(e, "LE", R01S_LVL_H);
    drive_d(e, 0xA5);
    r01s_entity_eval(e);
    expect_true(sense_q(e) == 0xA5, "transparent Q=D");
    expect_true(r01s_sn74hc573_peek_q(&chip) == 0xA5, "latched mirror");

    /* Latch: LE -> L, then D changes => Q holds */
    r01s_entity_drive(e, "LE", R01S_LVL_L);
    r01s_entity_eval(e);
    drive_d(e, 0x00);
    r01s_entity_eval(e);
    expect_true(sense_q(e) == 0xA5, "hold after LE low");

    /* OE high => Hi-Z */
    r01s_entity_drive(e, "OE", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Q") == R01S_LVL_Z, "OE Hi-Z 1Q");
    expect_true(r01s_entity_sense(e, "8Q") == R01S_LVL_Z, "OE Hi-Z 8Q");

    /* Re-enable: still held value */
    r01s_entity_drive(e, "OE", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(sense_q(e) == 0xA5, "Q restored after OE");

    return test_done("test_sn74hc573");
}
