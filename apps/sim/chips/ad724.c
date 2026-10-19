#include "ad724.h"

#include "retr01_sim/bus.h"

#include <string.h>

static int ad724_powered(R01sEntity *e) {
    return r01s_level_is_high(r01s_entity_sense(e, "VCC1")) &&
           r01s_level_is_low(r01s_entity_sense(e, "GND1"));
}

static void ad724_reset(R01sEntity *e) {
    r01s_entity_drive(e, "CVOUT", R01S_LVL_Z);
}

static void ad724_eval(R01sEntity *e) {
    R01sLevel present;

    if (!ad724_powered(e)) {
        r01s_entity_drive(e, "CVOUT", R01S_LVL_Z);
        return;
    }
    present = r01s_level_is_high(r01s_entity_sense(e, "RIN")) ||
              r01s_level_is_high(r01s_entity_sense(e, "GIN")) ||
              r01s_level_is_high(r01s_entity_sense(e, "BIN"))
                  ? R01S_LVL_H
                  : R01S_LVL_L;
    r01s_entity_drive(e, "CVOUT", present);
}

static void ad724_tick(R01sEntity *e) {
    (void)e;
}

static void ad724_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable AD724_VT = {ad724_reset, ad724_eval, ad724_tick, ad724_destroy};

void r01s_ad724_init(R01sAd724 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &AD724_VT, "CXA2075M", refdes ? refdes : "U2075");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "GND1", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 2, "RIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "GIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "BIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "SCIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "NPIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "SYNCIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "VCC1", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 19, "VCC2", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 20, "CVOUT", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 24, "GND2", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_ad724_entity(R01sAd724 *chip) {
    return chip ? &chip->base : NULL;
}
