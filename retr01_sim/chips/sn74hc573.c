#include "sn74hc573.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void hc573_hiz_q(R01sEntity *e) {
    int i;
    char name[8];
    for (i = 1; i <= 8; i++) {
        snprintf(name, sizeof(name), "%dQ", i);
        r01s_entity_drive(e, name, R01S_LVL_Z);
    }
}

static uint8_t hc573_read_d(R01sEntity *e) {
    int i;
    uint8_t v = 0;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dD", i + 1);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

static void hc573_drive_q(R01sEntity *e, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dQ", i + 1);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc573_reset(R01sEntity *e) {
    R01sSn74hc573 *c = (R01sSn74hc573 *)e;
    c->latched = 0;
    hc573_hiz_q(e);
}

static void hc573_eval(R01sEntity *e) {
    R01sSn74hc573 *c = (R01sSn74hc573 *)e;
    R01sLevel oe = r01s_entity_sense(e, "OE");
    R01sLevel le = r01s_entity_sense(e, "LE");

    if (r01s_level_is_high(oe)) {
        hc573_hiz_q(e);
        return;
    }
    if (r01s_level_is_high(le)) {
        c->latched = hc573_read_d(e);
    }
    hc573_drive_q(e, c->latched);
}

static void hc573_tick(R01sEntity *e) {
    (void)e;
}

static void hc573_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC573_VT = {hc573_reset, hc573_eval, hc573_tick, hc573_destroy};

void r01s_sn74hc573_init(R01sSn74hc573 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC573_VT, "SN74HC573", refdes ? refdes : "U?");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "OE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "1D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "2D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "3D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "4D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "5D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "6D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "7D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "8D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "LE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "8Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "7Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "6Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 15, "5Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "4Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 17, "3Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 18, "2Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 19, "1Q", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 20, 56);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc573_entity(R01sSn74hc573 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_sn74hc573_peek_q(const R01sSn74hc573 *chip) {
    return chip ? chip->latched : 0;
}
