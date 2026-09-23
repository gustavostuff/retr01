#include "osc_dot.h"

#include "netlist_sim/bus.h"
#include "netlist_sim/passive.h"

#include <string.h>

static void osc_dot_reset(NsEntity *e) {
    ns_entity_drive(e, "DOT", NS_LVL_L);
}

static void osc_dot_eval(NsEntity *e) {
    NsLevel vdd = ns_entity_sense(e, "VDD");
    NsLevel oe = ns_entity_sense(e, "OE#");
    if (!ns_level_is_high(vdd) || ns_level_is_low(oe)) {
        ns_entity_drive(e, "DOT", NS_LVL_Z);
    }
}

static void osc_dot_tick(NsEntity *e) {
    NsLevel vdd = ns_entity_sense(e, "VDD");
    NsLevel oe = ns_entity_sense(e, "OE#");
    if (!ns_level_is_high(vdd) || ns_level_is_low(oe)) {
        ns_entity_drive(e, "DOT", NS_LVL_Z);
        return;
    }
    if (ns_entity_sense(e, "DOT") == NS_LVL_H) {
        ns_entity_drive(e, "DOT", NS_LVL_L);
    } else {
        ns_entity_drive(e, "DOT", NS_LVL_H);
    }
}

static void osc_dot_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable OSC_DOT_VT = {osc_dot_reset, osc_dot_eval, osc_dot_tick, osc_dot_destroy};

void r01a_osc_dot_init(R01aOscDot *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &OSC_DOT_VT, "OSC_DOT", refdes ? refdes : "Y2");
    chip->base.impl = chip;
    ns_entity_add_pin(&chip->base, 1, "OE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 7, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 8, "DOT", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 14, "VDD", NS_PIN_IN);
    ns_entity_set_glyph(&chip->base, NS_ENTITY_VIS_OSC, 1, 1);
    ns_osc4legs_sync_aabb(&chip->base);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_osc_dot_entity(R01aOscDot *chip) {
    return chip ? &chip->base : NULL;
}
