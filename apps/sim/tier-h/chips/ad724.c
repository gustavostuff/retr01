#include "ad724.h"

#include "retr01_sim/bus.h"

#include <string.h>

static int ad724_powered(R01sEntity *e) {
    return r01s_level_is_high(r01s_entity_sense(e, "APOS")) &&
           r01s_level_is_high(r01s_entity_sense(e, "DPOS")) &&
           r01s_level_is_low(r01s_entity_sense(e, "AGND")) &&
           r01s_level_is_low(r01s_entity_sense(e, "DGND"));
}

static void ad724_reset(R01sEntity *e) {
    r01s_entity_drive(e, "COMP", R01S_LVL_Z);
    r01s_entity_drive(e, "LUMA", R01S_LVL_Z);
    r01s_entity_drive(e, "CRMA", R01S_LVL_Z);
}

static void ad724_eval(R01sEntity *e) {
    R01sLevel present;

    if (!ad724_powered(e) || !r01s_level_is_high(r01s_entity_sense(e, "ENCD"))) {
        r01s_entity_drive(e, "COMP", R01S_LVL_Z);
        r01s_entity_drive(e, "LUMA", R01S_LVL_Z);
        r01s_entity_drive(e, "CRMA", R01S_LVL_Z);
        return;
    }
    present = r01s_level_is_high(r01s_entity_sense(e, "RIN")) ||
              r01s_level_is_high(r01s_entity_sense(e, "GIN")) ||
              r01s_level_is_high(r01s_entity_sense(e, "BIN"))
                  ? R01S_LVL_H
                  : R01S_LVL_L;
    r01s_entity_drive(e, "COMP", present);
    r01s_entity_drive(e, "LUMA", present);
    r01s_entity_drive(e, "CRMA", present);
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
    r01s_entity_init(&chip->base, &AD724_VT, "AD724", refdes ? refdes : "U725");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "STND", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "AGND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 3, "FIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "APOS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 5, "ENCD", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "RIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "GIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "BIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "CRMA", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 10, "COMP", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 11, "LUMA", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "SELECT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "DGND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 14, "DPOS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 15, "VSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 16, "HSYNC", R01S_PIN_IN);
    r01s_entity_set_dip_mm(&chip->base, 16, 10, 4);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_ad724_entity(R01sAd724 *chip) {
    return chip ? &chip->base : NULL;
}
