#include "osc_fsc.h"

#include "netlist_sim/bus.h"

#include <string.h>

static void osc_fsc_reset(NsEntity *e) {
    ns_entity_drive(e, "FSC", NS_LVL_L);
}

static void osc_fsc_eval(NsEntity *e) {
    NsLevel vdd = ns_entity_sense(e, "VDD");
    NsLevel oe = ns_entity_sense(e, "OE#");
    if (!ns_level_is_high(vdd) || ns_level_is_low(oe)) {
        ns_entity_drive(e, "FSC", NS_LVL_Z);
    }
}

static void osc_fsc_tick(NsEntity *e) {
    NsLevel vdd = ns_entity_sense(e, "VDD");
    NsLevel oe = ns_entity_sense(e, "OE#");
    if (!ns_level_is_high(vdd) || ns_level_is_low(oe)) {
        ns_entity_drive(e, "FSC", NS_LVL_Z);
        return;
    }
    if (ns_entity_sense(e, "FSC") == NS_LVL_H) {
        ns_entity_drive(e, "FSC", NS_LVL_L);
    } else {
        ns_entity_drive(e, "FSC", NS_LVL_H);
    }
}

static void osc_fsc_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable OSC_FSC_VT = {osc_fsc_reset, osc_fsc_eval, osc_fsc_tick, osc_fsc_destroy};

void r01a_osc_fsc_init(R01aOscFsc *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &OSC_FSC_VT, "OSC_FSC", refdes ? refdes : "Y3");
    chip->base.impl = chip;
    ns_entity_add_pin(&chip->base, 1, "OE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 5, "FSC", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 8, "VDD", NS_PIN_IN);
    ns_entity_set_glyph(&chip->base, NS_ENTITY_VIS_OSC, 30, 30);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_osc_fsc_entity(R01aOscFsc *chip) {
    return chip ? &chip->base : NULL;
}
