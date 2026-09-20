#include "pwr5v.h"

#include "netlist_sim/bus.h"

#include <string.h>

static void pwr5v_reset(NsEntity *e) {
    R01aPwr5v *c = (R01aPwr5v *)e;
    c->power_ok = 0;
    ns_entity_drive(e, "VDD", NS_LVL_Z);
}

static void pwr5v_eval(NsEntity *e) {
    R01aPwr5v *c = (R01aPwr5v *)e;
    NsLevel vin = ns_entity_sense(e, "VIN");
    NsLevel en = ns_entity_sense(e, "EN");
    int enabled = !ns_level_is_low(en);
    if (ns_level_is_high(vin) && enabled) {
        ns_entity_drive(e, "VDD", NS_LVL_H);
        c->power_ok = 1;
    } else {
        ns_entity_drive(e, "VDD", NS_LVL_L);
        c->power_ok = 0;
    }
}

static void pwr5v_tick(NsEntity *e) {
    (void)e;
}

static void pwr5v_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable PWR5V_VT = {pwr5v_reset, pwr5v_eval, pwr5v_tick, pwr5v_destroy};

void r01a_pwr5v_init(R01aPwr5v *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &PWR5V_VT, "PWR5V", refdes ? refdes : "PS1");
    chip->base.impl = chip;
    ns_entity_add_pin(&chip->base, 1, "VIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "EN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 3, "VDD", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 4, "GND", NS_PIN_PWR);
    ns_entity_set_glyph(&chip->base, NS_ENTITY_VIS_PWR, 30, 45);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_pwr5v_entity(R01aPwr5v *chip) {
    return chip ? &chip->base : NULL;
}
