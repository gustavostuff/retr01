#include "osc8m.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void osc8m_reset(R01sEntity *e) {
    R01sOsc8m *c = (R01sOsc8m *)e;
    c->half_cycles = 0;
    r01s_entity_drive(e, "PHI2", R01S_LVL_L);
}

static void osc8m_eval(R01sEntity *e) {
    R01sLevel vdd = r01s_entity_sense(e, "VDD");
    R01sLevel oe = r01s_entity_sense(e, "OE#");
    if (!r01s_level_is_high(vdd) || r01s_level_is_low(oe)) {
        r01s_entity_drive(e, "PHI2", R01S_LVL_Z);
    }
}

static void osc8m_tick(R01sEntity *e) {
    R01sOsc8m *c = (R01sOsc8m *)e;
    R01sLevel vdd = r01s_entity_sense(e, "VDD");
    R01sLevel oe = r01s_entity_sense(e, "OE#");
    if (!r01s_level_is_high(vdd) || r01s_level_is_low(oe)) {
        r01s_entity_drive(e, "PHI2", R01S_LVL_Z);
        return;
    }
    c->half_cycles++;
    if (r01s_entity_sense(e, "PHI2") == R01S_LVL_H) {
        r01s_entity_drive(e, "PHI2", R01S_LVL_L);
    } else {
        r01s_entity_drive(e, "PHI2", R01S_LVL_H);
    }
}

static void osc8m_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable OSC8M_VT = {osc8m_reset, osc8m_eval, osc8m_tick, osc8m_destroy};

void r01s_osc8m_init(R01sOsc8m *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &OSC8M_VT, "OSC8M", refdes ? refdes : "Y1");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 5, "PHI2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "VDD", R01S_PIN_IN);
    r01s_entity_set_dip(&chip->base, 8, 40);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_osc8m_entity(R01sOsc8m *chip) {
    return chip ? &chip->base : NULL;
}
