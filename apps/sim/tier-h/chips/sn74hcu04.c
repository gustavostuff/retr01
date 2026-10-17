#include "sn74hcu04.h"

#include "discrete_ic/bus.h"

#include <string.h>

static void hcu04_reset(NsEntity *e) {
    R01aSn74hcu04 *c = (R01aSn74hcu04 *)e;
    c->osc_phase = NS_LVL_L;
    ns_entity_drive(e, "1Y", NS_LVL_L);
    ns_entity_drive(e, "2Y", NS_LVL_L);
    ns_entity_drive(e, "3Y", NS_LVL_L);
    ns_entity_drive(e, "4Y", NS_LVL_L);
    ns_entity_drive(e, "5Y", NS_LVL_L);
    ns_entity_drive(e, "6Y", NS_LVL_L);
}

static void hcu04_eval(NsEntity *e) {
    R01aSn74hcu04 *c = (R01aSn74hcu04 *)e;
    NsLevel vcc = ns_entity_sense(e, "VCC");
    NsLevel gnd = ns_entity_sense(e, "GND");
    NsLevel a1;
    NsLevel a2;
    NsLevel a3;
    NsLevel a4;
    NsLevel a5;
    NsLevel a6;

    if (!ns_level_is_high(vcc) || ns_level_is_high(gnd)) {
        ns_entity_drive(e, "1Y", NS_LVL_Z);
        ns_entity_drive(e, "2Y", NS_LVL_Z);
        ns_entity_drive(e, "3Y", NS_LVL_Z);
        ns_entity_drive(e, "4Y", NS_LVL_Z);
        ns_entity_drive(e, "5Y", NS_LVL_Z);
        ns_entity_drive(e, "6Y", NS_LVL_Z);
        return;
    }

    /* Gate 1: If driven externally to definitive logic level, invert; otherwise output osc_phase */
    a1 = ns_entity_sense(e, "1A");
    if (a1 == NS_LVL_H) {
        ns_entity_drive(e, "1Y", NS_LVL_L);
    } else if (a1 == NS_LVL_L) {
        ns_entity_drive(e, "1Y", NS_LVL_H);
    } else {
        ns_entity_drive(e, "1Y", c->osc_phase);
    }

    /* Gate 2: Inverter buffer (2A -> 2Y) */
    a2 = ns_entity_sense(e, "2A");
    ns_entity_drive(e, "2Y", ns_level_is_high(a2) ? NS_LVL_L : (ns_level_is_low(a2) ? NS_LVL_H : NS_LVL_X));

    /* Gate 3: (3A -> 3Y) */
    a3 = ns_entity_sense(e, "3A");
    ns_entity_drive(e, "3Y", ns_level_is_high(a3) ? NS_LVL_L : (ns_level_is_low(a3) ? NS_LVL_H : NS_LVL_X));

    /* Gate 4: (4A -> 4Y) */
    a4 = ns_entity_sense(e, "4A");
    ns_entity_drive(e, "4Y", ns_level_is_high(a4) ? NS_LVL_L : (ns_level_is_low(a4) ? NS_LVL_H : NS_LVL_X));

    /* Gate 5: (5A -> 5Y) */
    a5 = ns_entity_sense(e, "5A");
    ns_entity_drive(e, "5Y", ns_level_is_high(a5) ? NS_LVL_L : (ns_level_is_low(a5) ? NS_LVL_H : NS_LVL_X));

    /* Gate 6: (6A -> 6Y) */
    a6 = ns_entity_sense(e, "6A");
    ns_entity_drive(e, "6Y", ns_level_is_high(a6) ? NS_LVL_L : (ns_level_is_low(a6) ? NS_LVL_H : NS_LVL_X));
}

static void hcu04_tick(NsEntity *e) {
    R01aSn74hcu04 *c = (R01aSn74hcu04 *)e;
    NsLevel vcc = ns_entity_sense(e, "VCC");
    NsLevel gnd = ns_entity_sense(e, "GND");
    if (!ns_level_is_high(vcc) || ns_level_is_high(gnd)) {
        return;
    }
    c->osc_phase = (c->osc_phase == NS_LVL_H) ? NS_LVL_L : NS_LVL_H;
    hcu04_eval(e);
}

static void hcu04_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable HCU04_VT = {hcu04_reset, hcu04_eval, hcu04_tick, hcu04_destroy};

void r01a_sn74hcu04_init(R01aSn74hcu04 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &HCU04_VT, "74HCU04", refdes ? refdes : "U04");
    chip->base.impl = chip;

    ns_entity_add_pin(&chip->base, 1, "1A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "1Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 3, "2A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "2Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 5, "3A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 6, "3Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 7, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 8, "4Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 9, "4A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 10, "5Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 11, "5A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 12, "6Y", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 13, "6A", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 14, "VCC", NS_PIN_PWR);

    ns_entity_set_dip(&chip->base, 14);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_sn74hcu04_entity(R01aSn74hcu04 *chip) {
    return chip ? &chip->base : NULL;
}
