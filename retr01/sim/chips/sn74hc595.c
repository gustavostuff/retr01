#include "sn74hc595.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void hc595_drive_q(R01sEntity *e, uint8_t v) {
    int i;
    char name[4];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "Q%d", i);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc595_hiz_q(R01sEntity *e) {
    int i;
    char name[4];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "Q%d", i);
        r01s_entity_drive(e, name, R01S_LVL_Z);
    }
}

static void hc595_reset(R01sEntity *e) {
    R01sSn74hc595 *c = (R01sSn74hc595 *)e;
    c->shift = 0;
    c->latched = 0;
    c->srclk_prev = R01S_LVL_L;
    c->rclk_prev = R01S_LVL_L;
    hc595_hiz_q(e);
    r01s_entity_drive(e, "Q7S", R01S_LVL_L);
}

static void hc595_eval(R01sEntity *e) {
    R01sSn74hc595 *c = (R01sSn74hc595 *)e;
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int srclk_rise = 0;
    int rclk_rise = 0;
    R01sLevel srclk = r01s_entity_sense(e, "SRCLK");
    R01sLevel rclk = r01s_entity_sense(e, "RCLK");

    if (!r01s_level_is_high(r01s_entity_sense(e, "SRCLR#"))) {
        c->shift = 0;
    }
    if (r01s_level_is_low(c->srclk_prev) && r01s_level_is_high(srclk)) {
        srclk_rise = 1;
    }
    if (r01s_level_is_low(c->rclk_prev) && r01s_level_is_high(rclk)) {
        rclk_rise = 1;
    }
    c->srclk_prev = srclk;
    c->rclk_prev = rclk;

    if (srclk_rise) {
        int ser = r01s_level_is_high(r01s_entity_sense(e, "SER"));
        c->shift = (uint8_t)((c->shift << 1) | (ser ? 1u : 0u));
    }
    if (rclk_rise) {
        c->latched = c->shift;
    }

    r01s_entity_drive(e, "Q7S", (c->shift & 0x80u) ? R01S_LVL_H : R01S_LVL_L);

    if (!oe) {
        hc595_hiz_q(e);
        return;
    }
    hc595_drive_q(e, c->latched);
}

static void hc595_tick(R01sEntity *e) {
    (void)e;
}

static void hc595_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC595_VT = {hc595_reset, hc595_eval, hc595_tick, hc595_destroy};

void r01s_sn74hc595_init(R01sSn74hc595 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC595_VT, "SN74HC595", refdes ? refdes : "U?");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "QB", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 2, "Q0", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 3, "Q1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "Q2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 5, "Q3", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 6, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 7, "Q4", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "Q5", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 9, "Q6", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 10, "Q7", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 11, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 12, "RCLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "SRCLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "SER", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 15, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 16, "SRCLR#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 17, "Q7S", R01S_PIN_OUT);
    r01s_entity_set_dip(&chip->base, 16);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc595_entity(R01sSn74hc595 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_sn74hc595_latched(const R01sSn74hc595 *chip) {
    return chip ? chip->latched : 0;
}
