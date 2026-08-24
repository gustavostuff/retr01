#include "osc_dot.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void osc_dot_reset(R01sEntity *e) {
    R01sOscDot *c = (R01sOscDot *)e;
    c->half_cycles = 0;
    r01s_entity_drive(e, "DOT", R01S_LVL_L);
}

static void osc_dot_eval(R01sEntity *e) {
    R01sLevel vdd = r01s_entity_sense(e, "VDD");
    R01sLevel oe = r01s_entity_sense(e, "OE#");
    if (!r01s_level_is_high(vdd) || r01s_level_is_low(oe)) {
        r01s_entity_drive(e, "DOT", R01S_LVL_Z);
    }
}

static void osc_dot_tick(R01sEntity *e) {
    R01sOscDot *c = (R01sOscDot *)e;
    R01sLevel vdd = r01s_entity_sense(e, "VDD");
    R01sLevel oe = r01s_entity_sense(e, "OE#");
    if (!r01s_level_is_high(vdd) || r01s_level_is_low(oe)) {
        r01s_entity_drive(e, "DOT", R01S_LVL_Z);
        return;
    }
    c->half_cycles++;
    if (r01s_entity_sense(e, "DOT") == R01S_LVL_H) {
        r01s_entity_drive(e, "DOT", R01S_LVL_L);
    } else {
        r01s_entity_drive(e, "DOT", R01S_LVL_H);
    }
}

static void osc_dot_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable OSC_DOT_VT = {osc_dot_reset, osc_dot_eval, osc_dot_tick, osc_dot_destroy};

void r01s_osc_dot_init(R01sOscDot *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &OSC_DOT_VT, "OSC_DOT", refdes ? refdes : "Y2");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 5, "DOT", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "VDD", R01S_PIN_IN);
    /* Sized for 2x osc.png (25x17) plus pin / label margins. */
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_OSC, 58, 54);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_osc_dot_entity(R01sOscDot *chip) {
    return chip ? &chip->base : NULL;
}
