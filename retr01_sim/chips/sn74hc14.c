#include "sn74hc14.h"

#include "retr01_sim/bus.h"

#include <string.h>

static const struct {
    const char *a;
    const char *y;
} HC14_GATES[6] = {
    {"1A", "1Y"}, {"2A", "2Y"}, {"3A", "3Y"}, {"4A", "4Y"}, {"5A", "5Y"}, {"6A", "6Y"},
};

static void hc14_reset(R01sEntity *e) {
    int i;
    for (i = 0; i < 6; i++) {
        r01s_entity_drive(e, HC14_GATES[i].y, R01S_LVL_Z);
    }
}

static void hc14_eval(R01sEntity *e) {
    int i;
    for (i = 0; i < 6; i++) {
        R01sLevel a = r01s_entity_sense(e, HC14_GATES[i].a);
        if (a == R01S_LVL_Z || a == R01S_LVL_X) {
            r01s_entity_drive(e, HC14_GATES[i].y, R01S_LVL_X);
        } else if (r01s_level_is_high(a)) {
            r01s_entity_drive(e, HC14_GATES[i].y, R01S_LVL_L);
        } else {
            r01s_entity_drive(e, HC14_GATES[i].y, R01S_LVL_H);
        }
    }
}

static void hc14_tick(R01sEntity *e) {
    (void)e;
}

static void hc14_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC14_VT = {hc14_reset, hc14_eval, hc14_tick, hc14_destroy};

void r01s_sn74hc14_init(R01sSn74hc14 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC14_VT, "SN74HC14", refdes ? refdes : "U?");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "1A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "1Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 3, "2A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "2Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 5, "3A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "3Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 7, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 8, "4Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 9, "4A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "5Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 11, "5A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "6Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "6A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 14, 48, 100);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc14_entity(R01sSn74hc14 *chip) {
    return chip ? &chip->base : NULL;
}
