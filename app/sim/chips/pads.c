#include "pads.h"

#include "retr01_sim/bus.h"

#include <string.h>

static int pads_port_select(const R01sEntity *e) {
    return r01s_level_is_high(r01s_entity_sense(e, "A0")) ? 1 : 0;
}

static void pads_drive_dq(R01sEntity *e, uint8_t bits) {
    r01s_bus_write(e, "DQ", 8, bits);
}

static void pads_preview_dq(R01sEntity *e, R01sPads *c) {
    pads_drive_dq(e, c->port[pads_port_select(e)]);
}

static void pads_reset(R01sEntity *e) {
    R01sPads *c = (R01sPads *)e;
    c->port[0] = 0;
    c->port[1] = 0;
    r01s_bus_hiz(e, "DQ", 8);
}

static void pads_eval(R01sEntity *e) {
    R01sPads *c = (R01sPads *)e;
    int sel;

    if (!r01s_level_is_low(r01s_entity_sense(e, "CE#")) || !r01s_level_is_low(r01s_entity_sense(e, "OE#"))) {
        /* Not on the CPU bus -- still show live host input on package pins (sim preview). */
        pads_preview_dq(e, c);
        return;
    }
    sel = pads_port_select(e);
    pads_drive_dq(e, c->port[sel]);
}

static void pads_tick(R01sEntity *e) {
    (void)e;
}

static void pads_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable PADS_VT = {pads_reset, pads_eval, pads_tick, pads_destroy};

void r01s_pads_init(R01sPads *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &PADS_VT, "PADS", refdes ? refdes : "PAD");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 5, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "DQ2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 7, "DQ3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "DQ4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 10, "DQ5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 11, "DQ6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 12, "DQ7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 14, "NC", R01S_PIN_NC);
    r01s_entity_set_dip(&chip->base, 14);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_pads_entity(R01sPads *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_pads_set(R01sPads *chip, int port, uint8_t bits) {
    if (!chip || port < 0 || port > 1) {
        return;
    }
    chip->port[port] = bits;
}

uint8_t r01s_pads_get(const R01sPads *chip, int port) {
    if (!chip || port < 0 || port > 1) {
        return 0;
    }
    return chip->port[port];
}

void r01s_pads_refresh_preview(R01sPads *chip) {
    R01sEntity *e;
    if (!chip) {
        return;
    }
    e = r01s_pads_entity(chip);
    r01s_entity_eval(e);
}
