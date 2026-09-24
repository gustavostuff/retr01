#include "sn74hc573.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void hc573_drive_q(R01sSn74hc573 *c) {
    R01sEntity *e = &c->base;
    int i;
    if (!r01s_level_is_low(r01s_entity_sense(e, "OE#"))) {
        r01s_bus_hiz(e, "Q", 8);
        return;
    }
    for (i = 0; i < 8; i++) {
        char qn[4];
        snprintf(qn, sizeof(qn), "Q%d", i);
        r01s_entity_drive(e, qn, (c->latched & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc573_reset(R01sEntity *e) {
    R01sSn74hc573 *c = (R01sSn74hc573 *)e;
    c->latched = 0;
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "LE", R01S_LVL_L);
    hc573_drive_q(c);
}

static void hc573_eval(R01sEntity *e) {
    R01sSn74hc573 *c = (R01sSn74hc573 *)e;
    if (r01s_level_is_high(r01s_entity_sense(e, "LE"))) {
        c->latched = (uint8_t)r01s_bus_read(e, "D", 8);
    }
    hc573_drive_q(c);
}

static void hc573_tick(R01sEntity *e) {
    (void)e;
}

static void hc573_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC573_VT = {hc573_reset, hc573_eval, hc573_tick, hc573_destroy};

void r01s_sn74hc573_init(R01sSn74hc573 *chip, const char *refdes) {
    static const char *const D_NAMES[8] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
    static const char *const Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC573_VT, "SN74HC573", refdes ? refdes : "U573");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "OE#", R01S_PIN_IN);
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 2 + i, D_NAMES[i], R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 12 + i, Q_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 10, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "LE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 20);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc573_entity(R01sSn74hc573 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_sn74hc573_q(const R01sSn74hc573 *chip) {
    return chip ? chip->latched : 0;
}
