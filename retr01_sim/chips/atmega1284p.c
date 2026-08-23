#include "atmega1284p.h"

#include "retr01_sim/bus.h"

#include <string.h>

static int port_is_data(const R01sEntity *e) {
    return r01s_level_is_high(r01s_entity_sense(e, "A0"));
}

static void mcu_reset(R01sEntity *e) {
    R01sAtmega1284p *c = (R01sAtmega1284p *)e;
    memset(c->oam, 0, sizeof(c->oam));
    memset(c->eeprom_mb, 0, sizeof(c->eeprom_mb));
    c->oam_addr = 0;
    c->last_dq = 0;
    c->we_prev = 0;
    c->oe_prev = 0;
    c->clk_ticks = 0;
    c->alive = 0;
    r01s_bus_hiz(e, "DQ", 8);
}

static void mcu_eval(R01sEntity *e) {
    R01sAtmega1284p *c = (R01sAtmega1284p *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_low(r01s_entity_sense(e, "WE#"));
    int data = port_is_data(e);
    uint8_t v;

    if (!ce) {
        c->we_prev = 0;
        c->oe_prev = 0;
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }

    /* Edge-trigger so settle-loop re-evals do not multi-inc OAM. */
    if (we && !c->we_prev) {
        v = (uint8_t)r01s_bus_read(e, "DQ", 8);
        if (data) {
            c->oam[c->oam_addr++] = v;
        } else {
            c->oam_addr = v;
        }
    }
    c->we_prev = we;

    if (oe && !c->oe_prev) {
        if (data) {
            c->last_dq = c->oam[c->oam_addr++];
        } else {
            c->last_dq = c->oam_addr;
        }
    }
    c->oe_prev = oe;

    if (we) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    if (oe) {
        r01s_bus_write(e, "DQ", 8, c->last_dq);
        return;
    }
    r01s_bus_hiz(e, "DQ", 8);
}

static void mcu_tick(R01sEntity *e) {
    R01sAtmega1284p *c = (R01sAtmega1284p *)e;
    /* Sim-domain stub for 20 MHz — full LCM master tick lands with N/K polish. */
    if (r01s_level_is_high(r01s_entity_sense(e, "CLK"))) {
        c->clk_ticks++;
        c->alive = 1;
    }
    r01s_entity_drive(e, "RUN", c->alive ? R01S_LVL_H : R01S_LVL_L);
}

static void mcu_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable MCU1284_VT = {mcu_reset, mcu_eval, mcu_tick, mcu_destroy};

void r01s_atmega1284p_init(R01sAtmega1284p *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &MCU1284_VT, "ATMEGA1284P", refdes ? refdes : "U1284");
    chip->base.impl = chip;

    /* Simplified OAM port (not full AVR PDIP map — decode TBD on schematic). */
    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 7, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "DQ2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 9, "DQ3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 10, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 12, "DQ4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "DQ5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "DQ6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 15, "DQ7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 17, "HBLANK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 18, "RUN", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 19, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 20, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 21, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 22, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 23, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 24, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 25, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 26, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 27, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 28, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 29, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 30, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 31, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 32, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 33, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 34, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 35, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 36, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 37, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 38, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 39, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 40, "AVCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 40, 64);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atmega1284p_entity(R01sAtmega1284p *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_atmega1284p_oam_addr(const R01sAtmega1284p *chip) {
    return chip ? chip->oam_addr : 0;
}

uint8_t r01s_atmega1284p_oam_peek(const R01sAtmega1284p *chip, uint8_t addr) {
    return chip ? chip->oam[addr] : 0;
}

void r01s_atmega1284p_oam_poke(R01sAtmega1284p *chip, uint8_t addr, uint8_t data) {
    if (chip) {
        chip->oam[addr] = data;
    }
}

uint8_t r01s_atmega1284p_eeprom_peek(const R01sAtmega1284p *chip, unsigned i) {
    if (!chip || i >= R01S_MCU1284_EEPROM_MAILBOX) {
        return 0;
    }
    return chip->eeprom_mb[i];
}

void r01s_atmega1284p_eeprom_poke(R01sAtmega1284p *chip, unsigned i, uint8_t data) {
    if (!chip || i >= R01S_MCU1284_EEPROM_MAILBOX) {
        return;
    }
    chip->eeprom_mb[i] = data;
}

uint32_t r01s_atmega1284p_clk_ticks(const R01sAtmega1284p *chip) {
    return chip ? chip->clk_ticks : 0;
}

int r01s_atmega1284p_alive(const R01sAtmega1284p *chip) {
    return chip && chip->alive;
}
