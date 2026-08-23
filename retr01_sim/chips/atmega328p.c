#include "atmega328p.h"

#include "retr01_sim/bus.h"

#include <string.h>

static unsigned apu_reg(const R01sEntity *e) {
    return (unsigned)(r01s_bus_read(e, "A", 5) & 0x1Fu);
}

static void apu_drive_pwm(R01sEntity *e, int hi) {
    r01s_entity_drive(e, "PWM", hi ? R01S_LVL_H : R01S_LVL_L);
}

static void apu_reset(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e;
    memset(c->regs, 0, sizeof(c->regs));
    c->phase = 0;
    c->pwm_hi_samples = 0;
    c->pwm_edges = 0;
    c->pwm_prev = R01S_LVL_L;
    r01s_bus_hiz(e, "DQ", 8);
    apu_drive_pwm(e, 0);
}

static void apu_eval(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_low(r01s_entity_sense(e, "WE#"));
    unsigned reg = apu_reg(e);

    if (!ce) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    if (we) {
        c->regs[reg] = (uint8_t)r01s_bus_read(e, "DQ", 8);
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    if (oe) {
        r01s_bus_write(e, "DQ", 8, c->regs[reg]);
        return;
    }
    r01s_bus_hiz(e, "DQ", 8);
}

static void apu_synth_tick(R01sAtmega328p *c, R01sEntity *e) {
    uint16_t period = r01s_atmega328p_period(c);
    int enable = r01s_atmega328p_enabled(c);
    int vol = (c->regs[0] >> 4) & 0x0F;
    int hi = 0;
    R01sLevel out;

    if (enable && period > 1u) {
        c->phase = (uint16_t)((c->phase + 1u) % period);
        /* Square: high for first half of period when volume > 0. */
        hi = (c->phase < (period / 2u)) && (vol > 0);
    } else {
        c->phase = 0;
    }
    out = hi ? R01S_LVL_H : R01S_LVL_L;
    if (out != c->pwm_prev && (out == R01S_LVL_H || c->pwm_prev == R01S_LVL_H)) {
        c->pwm_edges++;
    }
    if (hi) {
        c->pwm_hi_samples++;
    }
    c->pwm_prev = out;
    apu_drive_pwm(e, hi);
}

static void apu_tick(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e;
    apu_synth_tick(c, e);
}

static void apu_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable APU_VT = {apu_reset, apu_eval, apu_tick, apu_destroy};

void r01s_atmega328p_init(R01sAtmega328p *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &APU_VT, "ATMEGA328P", refdes ? refdes : "U328");
    chip->base.impl = chip;

    /* Simplified APU port (not full AVR PDIP map — decode TBD on schematic). */
    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "A1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "A2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "A3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "A4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "DQ2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 15, "DQ3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "DQ4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 17, "DQ5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 18, "DQ6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 19, "DQ7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 20, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 21, "PWM", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 22, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 23, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 24, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 25, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 26, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 27, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 28, "AVCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 28, 56);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atmega328p_entity(R01sAtmega328p *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_atmega328p_peek(const R01sAtmega328p *chip, unsigned reg) {
    if (!chip || reg >= R01S_APU_REGS) {
        return 0;
    }
    return chip->regs[reg];
}

void r01s_atmega328p_poke(R01sAtmega328p *chip, unsigned reg, uint8_t data) {
    if (!chip || reg >= R01S_APU_REGS) {
        return;
    }
    chip->regs[reg] = data;
}

int r01s_atmega328p_enabled(const R01sAtmega328p *chip) {
    return chip && (chip->regs[0] & 0x01u) != 0;
}

uint16_t r01s_atmega328p_period(const R01sAtmega328p *chip) {
    if (!chip) {
        return 0;
    }
    return (uint16_t)((chip->regs[1] | ((chip->regs[2] & 0x07u) << 8)) & 0x7FFu);
}

uint32_t r01s_atmega328p_pwm_edges(const R01sAtmega328p *chip) {
    return chip ? chip->pwm_edges : 0;
}

uint32_t r01s_atmega328p_pwm_hi_samples(const R01sAtmega328p *chip) {
    return chip ? chip->pwm_hi_samples : 0;
}
