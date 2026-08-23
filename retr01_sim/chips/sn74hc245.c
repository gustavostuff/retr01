#include "sn74hc245.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void hc245_hiz_side(R01sEntity *e, char side) {
    int i;
    char name[8];
    for (i = 1; i <= 8; i++) {
        snprintf(name, sizeof(name), "%c%d", side, i);
        r01s_entity_drive(e, name, R01S_LVL_Z);
    }
}

static uint8_t hc245_read_side(R01sEntity *e, char side) {
    int i;
    uint8_t v = 0;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%c%d", side, i + 1);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

static void hc245_drive_side(R01sEntity *e, char side, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%c%d", side, i + 1);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void hc245_reset(R01sEntity *e) {
    hc245_hiz_side(e, 'A');
    hc245_hiz_side(e, 'B');
}

static void hc245_eval(R01sEntity *e) {
    R01sLevel oe = r01s_entity_sense(e, "OE");
    R01sLevel dir = r01s_entity_sense(e, "DIR");

    if (!r01s_level_is_low(oe)) {
        /* Chip releases both ports; does not overwrite external drivers mid-net. */
        hc245_hiz_side(e, 'A');
        hc245_hiz_side(e, 'B');
        return;
    }
    if (r01s_level_is_high(dir)) {
        /* A inputs, B outputs — leave A levels alone. */
        hc245_drive_side(e, 'B', hc245_read_side(e, 'A'));
    } else {
        /* B inputs, A outputs — leave B levels alone. */
        hc245_drive_side(e, 'A', hc245_read_side(e, 'B'));
    }
}

static void hc245_tick(R01sEntity *e) {
    (void)e;
}

static void hc245_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable HC245_VT = {hc245_reset, hc245_eval, hc245_tick, hc245_destroy};

void r01s_sn74hc245_init(R01sSn74hc245 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &HC245_VT, "SN74HC245", refdes ? refdes : "U?");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "DIR", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "A1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 3, "A2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 4, "A3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 5, "A4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "A5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 7, "A6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "A7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 9, "A8", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 10, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "B8", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 12, "B7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "B6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "B5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 15, "B4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "B3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 17, "B2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 18, "B1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 19, "OE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 20, 56);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sn74hc245_entity(R01sSn74hc245 *chip) {
    return chip ? &chip->base : NULL;
}
