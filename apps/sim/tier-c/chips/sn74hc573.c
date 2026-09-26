#include "sn74hc573.h"

#include "discrete_ic/bus.h"

#include <stdio.h>
#include <string.h>

static void hc573_drive_q(R01aSn74hc573 *c) {
    NsEntity *e = &c->base;
    int i;
    if (!ns_level_is_low(ns_entity_sense(e, "OE#"))) {
        ns_bus_hiz(e, "Q", 8);
        return;
    }
    for (i = 0; i < 8; i++) {
        char qn[4];
        snprintf(qn, sizeof(qn), "Q%d", i);
        ns_entity_drive(e, qn, (c->latched & (1u << i)) ? NS_LVL_H : NS_LVL_L);
    }
}

static void hc573_reset(NsEntity *e) {
    R01aSn74hc573 *c = (R01aSn74hc573 *)e;
    c->latched = 0;
    ns_entity_drive(e, "OE#", NS_LVL_L);
    ns_entity_drive(e, "LE", NS_LVL_L);
    hc573_drive_q(c);
}

static void hc573_eval(NsEntity *e) {
    R01aSn74hc573 *c = (R01aSn74hc573 *)e;
    if (ns_level_is_high(ns_entity_sense(e, "LE"))) {
        c->latched = (uint8_t)ns_bus_read(e, "D", 8);
    }
    hc573_drive_q(c);
}

static void hc573_tick(NsEntity *e) {
    (void)e;
}

static void hc573_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable HC573_VT = {hc573_reset, hc573_eval, hc573_tick, hc573_destroy};

void r01a_sn74hc573_init(R01aSn74hc573 *chip, const char *refdes) {
    static const char *const D_NAMES[8] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
    static const char *const Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &HC573_VT, "SN74HC573", refdes ? refdes : "U573");
    chip->base.impl = chip;

    ns_entity_add_pin(&chip->base, 1, "OE#", NS_PIN_IN);
    for (i = 0; i < 8; i++) {
        ns_entity_add_pin(&chip->base, 2 + i, D_NAMES[i], NS_PIN_IN);
        ns_entity_add_pin(&chip->base, 12 + i, Q_NAMES[i], NS_PIN_OUT);
    }
    ns_entity_add_pin(&chip->base, 10, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 11, "LE", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 20, "VCC", NS_PIN_PWR);
    ns_entity_set_dip(&chip->base, 20);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_sn74hc573_entity(R01aSn74hc573 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01a_sn74hc573_q(const R01aSn74hc573 *chip) {
    return chip ? chip->latched : 0;
}
