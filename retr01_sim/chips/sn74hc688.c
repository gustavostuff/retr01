#include "sn74hc688.h"

#include "retr01_sim/bus.h"

#include <string.h>

static uint8_t hc688_read_bus(R01sEntity *e, int is_q) {
    static const char *const P_NAMES[8] = {"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};
    static const char *const Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};
    const char *const *names = is_q ? Q_NAMES : P_NAMES;
    int i;
    uint8_t v = 0;
    for (i = 0; i < 8; i++) {
        if (r01s_level_is_high(r01s_entity_sense(e, names[i]))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

static void hc688_reset(R01sEntity *e) {
    r01s_entity_drive(e, "EQ#", R01S_LVL_H);
}

static void hc688_eval(R01sEntity *e) {
    uint8_t p, q;
    if (r01s_level_is_high(r01s_entity_sense(e, "OE#"))) {
        r01s_entity_drive(e, "EQ#", R01S_LVL_H);
        return;
    }
    p = hc688_read_bus(e, 0);
    q = hc688_read_bus(e, 1);
    r01s_entity_drive(e, "EQ#", (p == q) ? R01S_LVL_L : R01S_LVL_H);
}

static void hc688_tick(R01sEntity *e) {
    (void)e;
}

static void hc688_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC688_VT = {hc688_reset, hc688_eval, hc688_tick, hc688_destroy};

void r01s_sn74hc688_init(R01sSn74hc688 *chip, const char *refdes) {
    static const char *const P_NAMES[8] = {"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};
    static const char *const Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC688_VT, "SN74HC688", refdes ? refdes : "U?");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "OE#", R01S_PIN_IN);
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 2 + i, P_NAMES[i], R01S_PIN_IN);
    }
    r01s_entity_add_pin(&chip->base, 10, "GND", R01S_PIN_PWR);
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 11 + i, Q_NAMES[i], R01S_PIN_IN);
    }
    r01s_entity_add_pin(&chip->base, 19, "EQ#", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 20, 56);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc688_entity(R01sSn74hc688 *chip) {
    return chip ? &chip->base : NULL;
}
