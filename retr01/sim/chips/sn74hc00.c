#include "sn74hc00.h"

#include "retr01_sim/bus.h"

#include <string.h>

static const struct {
    const char *a;
    const char *b;
    const char *y;
} HC00_GATES[4] = {
    {"1A", "1B", "1Y"},
    {"2A", "2B", "2Y"},
    {"3A", "3B", "3Y"},
    {"4A", "4B", "4Y"},
};

static R01sLevel gate2_nand(R01sLevel a, R01sLevel b) {
    if (a == R01S_LVL_Z || a == R01S_LVL_X || b == R01S_LVL_Z || b == R01S_LVL_X) {
        return R01S_LVL_X;
    }
    if (r01s_level_is_high(a) && r01s_level_is_high(b)) {
        return R01S_LVL_L;
    }
    return R01S_LVL_H;
}

static void hc00_reset(R01sEntity *e) {
    int i;
    for (i = 0; i < 4; i++) {
        r01s_entity_drive(e, HC00_GATES[i].y, R01S_LVL_Z);
    }
}

static void hc00_eval(R01sEntity *e) {
    int i;
    for (i = 0; i < 4; i++) {
        r01s_entity_drive(e, HC00_GATES[i].y,
                          gate2_nand(r01s_entity_sense(e, HC00_GATES[i].a),
                                     r01s_entity_sense(e, HC00_GATES[i].b)));
    }
}

static void hc00_tick(R01sEntity *e) {
    (void)e;
}

static void hc00_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC00_VT = {hc00_reset, hc00_eval, hc00_tick, hc00_destroy};

static void add_quad2_pins(R01sEntity *e) {
    r01s_entity_add_pin(e, 1, "1A", R01S_PIN_IN);
    r01s_entity_add_pin(e, 2, "1B", R01S_PIN_IN);
    r01s_entity_add_pin(e, 3, "1Y", R01S_PIN_OUT);
    r01s_entity_add_pin(e, 4, "2A", R01S_PIN_IN);
    r01s_entity_add_pin(e, 5, "2B", R01S_PIN_IN);
    r01s_entity_add_pin(e, 6, "2Y", R01S_PIN_OUT);
    r01s_entity_add_pin(e, 7, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(e, 8, "3Y", R01S_PIN_OUT);
    r01s_entity_add_pin(e, 9, "3A", R01S_PIN_IN);
    r01s_entity_add_pin(e, 10, "3B", R01S_PIN_IN);
    r01s_entity_add_pin(e, 11, "4Y", R01S_PIN_OUT);
    r01s_entity_add_pin(e, 12, "4A", R01S_PIN_IN);
    r01s_entity_add_pin(e, 13, "4B", R01S_PIN_IN);
    r01s_entity_add_pin(e, 14, "VCC", R01S_PIN_PWR);
}

void r01s_sn74hc00_init(R01sSn74hc00 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC00_VT, "SN74HC00", refdes ? refdes : "U?");
    chip->base.impl = chip;
    add_quad2_pins(&chip->base);
    r01s_entity_set_dip(&chip->base, 14);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc00_entity(R01sSn74hc00 *chip) {
    return chip ? &chip->base : NULL;
}
