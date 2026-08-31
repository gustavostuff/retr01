#include "sn74hc157.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static const struct {
    const char *a;
    const char *b;
    const char *y;
} HC157_GATES[4] = {
    {"1A", "1B", "1Y"},
    {"2A", "2B", "2Y"},
    {"3A", "3B", "3Y"},
    {"4A", "4B", "4Y"},
};

static void hc157_drive_y_nibble(R01sEntity *e, uint8_t y4) {
    int i;
    for (i = 0; i < 4; i++) {
        r01s_entity_drive(e, HC157_GATES[i].y, (y4 & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc157_reset(R01sEntity *e) {
    R01sSn74hc157 *c = (R01sSn74hc157 *)e;
    r01s_delay_u8_reset(&c->y_delay, 0);
    hc157_drive_y_nibble(e, 0);
}

static void hc157_eval(R01sEntity *e) {
    R01sSn74hc157 *c = (R01sSn74hc157 *)e;
    int i;
    R01sLevel g = r01s_entity_sense(e, "G#");
    R01sLevel ab = r01s_entity_sense(e, "AB");
    int select_b = r01s_level_is_high(ab);
    uint8_t ideal = 0;
    uint8_t y4;

    for (i = 0; i < 4; i++) {
        R01sLevel y;
        if (!r01s_level_is_low(g)) {
            y = R01S_LVL_L;
        } else {
            R01sLevel in = r01s_entity_sense(e, select_b ? HC157_GATES[i].b : HC157_GATES[i].a);
            if (in == R01S_LVL_Z || in == R01S_LVL_X) {
                /* Treat unknown as L for delay packing; X still rare on address mux. */
                y = R01S_LVL_L;
            } else {
                y = r01s_level_is_high(in) ? R01S_LVL_H : R01S_LVL_L;
            }
        }
        if (y == R01S_LVL_H) {
            ideal |= (uint8_t)(1u << i);
        }
    }
    y4 = r01s_delay_u8_update(&c->y_delay, ideal, r01s_timing_pin_tpd_ns(R01S_TPD_PART_HC157));
    hc157_drive_y_nibble(e, y4);
}

static void hc157_tick(R01sEntity *e) {
    (void)e;
}

static void hc157_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC157_VT = {hc157_reset, hc157_eval, hc157_tick, hc157_destroy};

void r01s_sn74hc157_init(R01sSn74hc157 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC157_VT, "SN74HC157", refdes ? refdes : "U?");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "AB", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "1A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "1B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "1Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 5, "2A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "2B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "2Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "3Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 10, "3B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "3A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "4Y", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "4B", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "4A", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 15, "G#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 16, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 16);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc157_entity(R01sSn74hc157 *chip) {
    return chip ? &chip->base : NULL;
}
