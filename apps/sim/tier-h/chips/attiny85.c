#include "attiny85.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void attiny85_apply_data(R01sEntity *e) {
    R01sAttiny85 *c = (R01sAttiny85 *)e->impl;
    if (!c) {
        return;
    }
    r01s_entity_drive(e, "DATA", c->driving_low ? R01S_LVL_L : R01S_LVL_Z);
}

static void attiny85_reset(R01sEntity *e) {
    R01sAttiny85 *c = (R01sAttiny85 *)e->impl;
    if (c) {
        c->buttons = 0;
        c->reply_armed = 0;
        c->reply_byte = 0;
        c->driving_low = 0;
    }
    r01s_entity_drive(e, "RESET#", R01S_LVL_H);
    r01s_entity_drive(e, "VCC", R01S_LVL_H);
    r01s_entity_drive(e, "GND", R01S_LVL_L);
    attiny85_apply_data(e);
}

static void attiny85_eval(R01sEntity *e) {
    attiny85_apply_data(e);
}

static void attiny85_tick(R01sEntity *e) {
    (void)e;
}

static void attiny85_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable ATTINY85_VT = {attiny85_reset, attiny85_eval, attiny85_tick, attiny85_destroy};

void r01s_attiny85_init(R01sAttiny85 *chip, const char *refdes, uint8_t poll_addr) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &ATTINY85_VT, "ATtiny85", refdes ? refdes : "U?");
    chip->base.impl = chip;
    chip->poll_addr = poll_addr;

    /* DIP-8: minimal pad-board pinout for sim (DATA = open-drain UART). */
    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "NC2", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 3, "NC3", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 4, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 5, "DATA", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "NC6", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 7, "NC7", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 8, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_attiny85_entity(R01sAttiny85 *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_attiny85_set_buttons(R01sAttiny85 *chip, uint8_t bits) {
    if (!chip) {
        return;
    }
    chip->buttons = bits;
}

uint8_t r01s_attiny85_get_buttons(const R01sAttiny85 *chip) {
    return chip ? chip->buttons : 0;
}

void r01s_attiny85_rx_byte(R01sAttiny85 *chip, uint8_t byte) {
    R01sEntity *e;
    if (!chip) {
        return;
    }
    e = &chip->base;
    /* Host is pulling the wire; pad releases while listening. */
    chip->driving_low = 0;
    chip->reply_armed = 0;
    attiny85_apply_data(e);

    if (byte != chip->poll_addr) {
        r01s_entity_eval(e);
        return;
    }
    /* Addressed: sample buttons and arm OD reply. */
    chip->reply_byte = chip->buttons;
    chip->reply_armed = 1;
    chip->driving_low = 1; /* activity pulse on DATA while reply pending */
    attiny85_apply_data(e);
    r01s_entity_eval(e);
}

int r01s_attiny85_take_reply(R01sAttiny85 *chip, uint8_t *out) {
    if (!chip || !chip->reply_armed) {
        return 0;
    }
    if (out) {
        *out = chip->reply_byte;
    }
    chip->reply_armed = 0;
    chip->driving_low = 0;
    attiny85_apply_data(&chip->base);
    r01s_entity_eval(&chip->base);
    return 1;
}
