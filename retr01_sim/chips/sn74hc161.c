#include "sn74hc161.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void hc161_drive_q(R01sEntity *e, uint8_t q) {
    r01s_entity_drive(e, "QA", (q & 1u) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "QB", (q & 2u) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "QC", (q & 4u) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "QD", (q & 8u) ? R01S_LVL_H : R01S_LVL_L);
}

static void hc161_drive_rco(R01sEntity *e, uint8_t q) {
    int ent = r01s_level_is_high(r01s_entity_sense(e, "ENT"));
    int full = (q & 0x0Fu) == 0x0Fu;
    r01s_entity_drive(e, "RCO", (ent && full) ? R01S_LVL_H : R01S_LVL_L);
}

static uint8_t hc161_read_abcd(R01sEntity *e) {
    uint8_t v = 0;
    if (r01s_level_is_high(r01s_entity_sense(e, "A"))) {
        v |= 1u;
    }
    if (r01s_level_is_high(r01s_entity_sense(e, "B"))) {
        v |= 2u;
    }
    if (r01s_level_is_high(r01s_entity_sense(e, "C"))) {
        v |= 4u;
    }
    if (r01s_level_is_high(r01s_entity_sense(e, "D"))) {
        v |= 8u;
    }
    return v;
}

static void hc161_reset(R01sEntity *e) {
    R01sSn74hc161 *c = (R01sSn74hc161 *)e;
    c->q = 0;
    c->clk_prev = R01S_LVL_L;
    hc161_drive_q(e, 0);
    hc161_drive_rco(e, 0);
}

static void hc161_eval(R01sEntity *e) {
    R01sSn74hc161 *c = (R01sSn74hc161 *)e;
    if (r01s_level_is_low(r01s_entity_sense(e, "CLR#"))) {
        c->q = 0;
    }
    hc161_drive_q(e, c->q);
    hc161_drive_rco(e, c->q);
}

static void hc161_tick(R01sEntity *e) {
    R01sSn74hc161 *c = (R01sSn74hc161 *)e;
    R01sLevel clk = r01s_entity_sense(e, "CLK");
    int rise = (clk == R01S_LVL_H && c->clk_prev != R01S_LVL_H);

    if (r01s_level_is_low(r01s_entity_sense(e, "CLR#"))) {
        c->q = 0;
        c->clk_prev = clk;
        hc161_drive_q(e, 0);
        hc161_drive_rco(e, 0);
        return;
    }

    if (rise) {
        if (r01s_level_is_low(r01s_entity_sense(e, "LOAD#"))) {
            c->q = hc161_read_abcd(e);
        } else if (r01s_level_is_high(r01s_entity_sense(e, "ENP")) &&
                   r01s_level_is_high(r01s_entity_sense(e, "ENT"))) {
            c->q = (uint8_t)((c->q + 1u) & 0x0Fu);
        }
    }
    c->clk_prev = clk;
    hc161_drive_q(e, c->q);
    hc161_drive_rco(e, c->q);
}

static void hc161_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC161_VT = {hc161_reset, hc161_eval, hc161_tick, hc161_destroy};

void r01s_sn74hc161_init(R01sSn74hc161 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC161_VT, "SN74HC161", refdes ? refdes : "U?");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "CLR#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "C", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "D", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "ENP", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "LOAD#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "ENT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "QD", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "QC", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "QB", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "QA", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 15, "RCO", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 16, 48);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc161_entity(R01sSn74hc161 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_sn74hc161_peek_q(const R01sSn74hc161 *chip) {
    return chip ? (uint8_t)(chip->q & 0x0Fu) : 0;
}
