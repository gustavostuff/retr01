#include "sn74hc574.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void hc574_drive_q(R01sSn74hc574 *c) {
    R01sEntity *e = &c->base;
    int i;
    if (!r01s_level_is_low(r01s_entity_sense(e, "OE#"))) {
        r01s_bus_hiz(e, "Q", 8);
        return;
    }
    for (i = 0; i < 8; i++) {
        char qn[4];
        snprintf(qn, sizeof(qn), "Q%d", i);
        r01s_entity_drive(e, qn, (c->q & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc574_reset(R01sEntity *e) {
    R01sSn74hc574 *c = (R01sSn74hc574 *)e;
    c->q = 0;
    c->clk_prev = 0;
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "CLK", R01S_LVL_L);
    hc574_drive_q(c);
}

static void hc574_eval(R01sEntity *e) {
    R01sSn74hc574 *c = (R01sSn74hc574 *)e;
    int clk = r01s_level_is_high(r01s_entity_sense(e, "CLK"));
    if (clk && !c->clk_prev) {
        c->q = (uint8_t)r01s_bus_read(e, "D", 8);
    }
    c->clk_prev = clk;
    hc574_drive_q(c);
}

static void hc574_tick(R01sEntity *e) {
    (void)e;
}

static void hc574_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC574_VT = {hc574_reset, hc574_eval, hc574_tick, hc574_destroy};

void r01s_sn74hc574_init(R01sSn74hc574 *chip, const char *refdes) {
    static const char *const D_NAMES[8] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
    static const char *const Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC574_VT, "SN74HC574", refdes ? refdes : "U574");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "OE#", R01S_PIN_IN);
    for (i = 0; i < 8; i++) {
        /* Datasheet interleaves D/Q; sim uses contiguous D then Q for bus helpers. */
        r01s_entity_add_pin(&chip->base, 2 + i, D_NAMES[i], R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 12 + i, Q_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 10, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip_mm(&chip->base, 20, 26, 6);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc574_entity(R01sSn74hc574 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_sn74hc574_q(const R01sSn74hc574 *chip) {
    return chip ? chip->q : 0;
}

void r01s_sn74hc574_force_q(R01sSn74hc574 *chip, uint8_t v) {
    if (!chip) {
        return;
    }
    chip->q = v;
    hc574_drive_q(chip);
}
