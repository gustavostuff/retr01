#include "sn74hc08.h"

#include "retr01_sim/bus.h"

#include <string.h>

static const struct {
    const char *a;
    const char *b;
    const char *y;
} HC08_GATES[4] = {
    {"1A", "1B", "1Y"},
    {"2A", "2B", "2Y"},
    {"3A", "3B", "3Y"},
    {"4A", "4B", "4Y"},
};

static R01sLevel gate2_and(R01sLevel a, R01sLevel b) {
    if (a == R01S_LVL_Z || a == R01S_LVL_X || b == R01S_LVL_Z || b == R01S_LVL_X) {
        return R01S_LVL_X;
    }
    if (r01s_level_is_high(a) && r01s_level_is_high(b)) {
        return R01S_LVL_H;
    }
    return R01S_LVL_L;
}

static void hc08_reset(R01sEntity *e) {
    int i;
    for (i = 0; i < 4; i++) {
        r01s_entity_drive(e, HC08_GATES[i].y, R01S_LVL_Z);
    }
}

static void hc08_eval(R01sEntity *e) {
    int i;
    for (i = 0; i < 4; i++) {
        r01s_entity_drive(e, HC08_GATES[i].y,
                          gate2_and(r01s_entity_sense(e, HC08_GATES[i].a),
                                    r01s_entity_sense(e, HC08_GATES[i].b)));
    }
}

static void hc08_tick(R01sEntity *e) {
    (void)e;
}

static void hc08_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC08_VT = {hc08_reset, hc08_eval, hc08_tick, hc08_destroy};

void r01s_sn74hc08_init(R01sSn74hc08 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC08_VT, "SN74HC08", refdes ? refdes : "U?");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "1A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "1B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "1Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "2A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "2B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "2Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 7, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 8, "3Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 9, "3A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "3B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "4Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "4A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "4B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 14);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc08_entity(R01sSn74hc08 *chip) {
    return chip ? &chip->base : NULL;
}
