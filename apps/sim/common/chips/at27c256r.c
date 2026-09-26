#include "at27c256r.h"

#include "discrete_ic/bus.h"
#include "r01_kit_palette.h"

#include <string.h>

static uint8_t quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t rr = (uint8_t)((r * 7 + 127) / 255);
    uint8_t gg = (uint8_t)((g * 7 + 127) / 255);
    uint8_t bb = (uint8_t)((b * 3 + 127) / 255);
    return (uint8_t)((rr << 5) | (gg << 2) | bb);
}

static int prom_addr(NsEntity *e) {
    static const char *const names[6] = {"A0", "A1", "A2", "A3", "A4", "A5"};
    int addr = 0;
    int i;
    for (i = 0; i < 6; i++) {
        if (ns_level_is_high(ns_entity_sense(e, names[i]))) {
            addr |= (1 << i);
        }
    }
    return addr & 63;
}

static void prom_reset(NsEntity *e) {
    ns_bus_hiz(e, "O", 8);
}

static void prom_eval(NsEntity *e) {
    R01aAt27c256r *c = (R01aAt27c256r *)e;
    int ce = ns_level_is_low(ns_entity_sense(e, "CE#"));
    int oe = ns_level_is_low(ns_entity_sense(e, "OE#"));

    if (!ce || !oe) {
        ns_bus_hiz(e, "O", 8);
        return;
    }
    ns_bus_write(e, "O", 8, c->mem[prom_addr(e)]);
}

static void prom_tick(NsEntity *e) {
    (void)e;
}

static void prom_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable PROM_VT = {prom_reset, prom_eval, prom_tick, prom_destroy};

void r01a_at27c256r_init(R01aAt27c256r *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &PROM_VT, "AT27C256R", refdes ? refdes : "U24");
    chip->base.impl = chip;
    ns_entity_add_pin(&chip->base, 1, "VPP", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "A12", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 3, "A7", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "A6", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 5, "A5", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 6, "A4", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 7, "A3", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 8, "A2", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 9, "A1", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 10, "A0", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 11, "O0", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 12, "O1", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 13, "O2", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 14, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 15, "O3", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 16, "O4", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 17, "O5", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 18, "O6", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 19, "O7", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 20, "CE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 21, "A10", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 22, "OE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 23, "A11", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 24, "A9", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 25, "A8", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 26, "A13", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 27, "PGM#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 28, "VCC", NS_PIN_PWR);
    /* 28P6 PDIP 600 mil (doc0014): D 36.7-37.3 mm, E1 13.5-14.0 mm molded width. */
    ns_entity_set_dip_mm(&chip->base, 28, 37, 14);
    r01a_at27c256r_load_kit(chip);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_at27c256r_entity(R01aAt27c256r *chip) {
    return chip ? &chip->base : NULL;
}

void r01a_at27c256r_load_kit(R01aAt27c256r *chip) {
    int i;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (!chip) {
        return;
    }
    for (i = 0; i < R01A_COLOR_PROM_ENTRIES; i++) {
        r01_kit_rgb(i, &r, &g, &b);
        chip->mem[i] = quantize_r3g3b2(r, g, b);
    }
}

uint8_t r01a_at27c256r_peek(const R01aAt27c256r *chip, int index) {
    if (!chip) {
        return 0;
    }
    return chip->mem[index & 63];
}
