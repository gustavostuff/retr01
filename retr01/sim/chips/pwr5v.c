#include "pwr5v.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void pwr5v_reset(R01sEntity *e) {
    R01sPwr5v *c = (R01sPwr5v *)e;
    c->power_ok = 0;
    r01s_entity_drive(e, "VDD", R01S_LVL_Z);
}

static void pwr5v_eval(R01sEntity *e) {
    R01sPwr5v *c = (R01sPwr5v *)e;
    R01sLevel vin = r01s_entity_sense(e, "VIN");
    R01sLevel en = r01s_entity_sense(e, "EN");
    int enabled = !r01s_level_is_low(en); /* Z or H = on */
    if (r01s_level_is_high(vin) && enabled) {
        r01s_entity_drive(e, "VDD", R01S_LVL_H);
        c->power_ok = 1;
    } else {
        r01s_entity_drive(e, "VDD", R01S_LVL_L);
        c->power_ok = 0;
    }
}

static void pwr5v_tick(R01sEntity *e) {
    (void)e;
}

static void pwr5v_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable PWR5V_VT = {pwr5v_reset, pwr5v_eval, pwr5v_tick, pwr5v_destroy};

void r01s_pwr5v_init(R01sPwr5v *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &PWR5V_VT, "PWR5V", refdes ? refdes : "PS1");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "VIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "EN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "VDD", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "GND", R01S_PIN_PWR);
    /* Sized for 2x battery.png (25x37) plus pin / label margins. */
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_PWR, 30, 45);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_pwr5v_entity(R01sPwr5v *chip) {
    return chip ? &chip->base : NULL;
}
