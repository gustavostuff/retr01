#include "atmega328p.h"

#include "retr01_sim/bus.h"

#include <string.h>

static const uint8_t APU_DEFAULT_WAVE[R01S_APU_CH_N] = {
    R01S_APU_WAVE_PULSE,    /* BGM1 lead */
    R01S_APU_WAVE_PULSE,    /* BGM2 harmony */
    R01S_APU_WAVE_TRIANGLE, /* BGM3 bass */
    R01S_APU_WAVE_NOISE,    /* BGM4 hats */
    R01S_APU_WAVE_DPCM,     /* BGM5 samples */
    R01S_APU_WAVE_PULSE,    /* SFX6 */
    R01S_APU_WAVE_PULSE,    /* SFX7 */
    R01S_APU_WAVE_NOISE,    /* SFX8 */
};

static unsigned apu_reg(const R01sEntity *e) {
    return (unsigned)(r01s_bus_read(e, "A", 5) & 0x1Fu);
}

static void apu_drive_pwm(R01sEntity *e, int hi) {
    r01s_entity_drive(e, "PWM", hi ? R01S_LVL_H : R01S_LVL_L);
}

static uint16_t pulse_threshold(uint16_t period, uint8_t duty) {
    uint32_t p = period ? period : 1u;
    switch (duty & 3u) {
    case 0:
        return (uint16_t)(p / 8u); /* 12.5% */
    case 1:
        return (uint16_t)(p / 4u); /* 25% */
    case 3:
        return (uint16_t)((p * 3u) / 4u); /* 75% */
    case 2:
    default:
        return (uint16_t)(p / 2u); /* 50% */
    }
}

static int16_t voice_sample(R01sApuVoice *v) {
    uint16_t period;
    int16_t amp;
    if (!v || !v->enable) {
        return 0;
    }
    period = v->period;
    if (period < 2u) {
        return 0;
    }
    v->phase = (uint16_t)((v->phase + 1u) % period);
    switch (v->wave) {
    case R01S_APU_WAVE_TRIANGLE: {
        /* Linear ramp 0..period/2 up, then down. Full amplitude when enabled. */
        uint16_t half = period / 2u;
        uint16_t ph = v->phase;
        int32_t y;
        if (half < 1u) {
            half = 1u;
        }
        if (ph < half) {
            y = ((int32_t)ph * 255) / (int32_t)half - 128;
        } else {
            y = 127 - (((int32_t)(ph - half) * 255) / (int32_t)half);
        }
        amp = (int16_t)y;
        break;
    }
    case R01S_APU_WAVE_NOISE: {
        /* 15-bit LFSR clocked once per period wrap (phase==0). */
        if (v->phase == 0u) {
            uint16_t l = v->noise_lfsr ? v->noise_lfsr : 1u;
            uint16_t bit = (uint16_t)(((l >> 0) ^ (l >> 1)) & 1u);
            v->noise_lfsr = (uint16_t)((l >> 1) | (bit << 14));
        }
        amp = (v->noise_lfsr & 1u) ? 127 : -128;
        if (v->vol == 0) {
            amp = 0;
        } else {
            amp = (int16_t)((amp * (int16_t)v->vol) / 15);
        }
        break;
    }
    case R01S_APU_WAVE_DPCM: {
        /* Stub: toggle accumulator every half period (stand-in until samples land). */
        if (v->phase == 0u) {
            v->dpcm_acc = (int8_t)(v->dpcm_acc + 2);
        } else if (v->phase == period / 2u) {
            v->dpcm_acc = (int8_t)(v->dpcm_acc - 2);
        }
        if (v->dpcm_acc > 63) {
            v->dpcm_acc = 63;
        }
        if (v->dpcm_acc < -64) {
            v->dpcm_acc = -64;
        }
        amp = (int16_t)(v->dpcm_acc * 2);
        if (v->vol == 0) {
            amp = 0;
        } else {
            amp = (int16_t)((amp * (int16_t)v->vol) / 15);
        }
        break;
    }
    case R01S_APU_WAVE_PULSE:
    default: {
        uint16_t thr = pulse_threshold(period, v->duty);
        int hi = (v->phase < thr);
        if (v->vol == 0) {
            amp = 0;
        } else {
            amp = hi ? (int16_t)((127 * (int16_t)v->vol) / 15) : (int16_t)((-128 * (int16_t)v->vol) / 15);
        }
        break;
    }
    }
    v->sample = amp;
    return amp;
}

static void sync_voice0_from_regs(R01sAtmega328p *c) {
    R01sApuVoice *v = &c->voice[0];
    v->wave = R01S_APU_WAVE_PULSE;
    v->enable = (c->regs[0] & 0x01u) != 0;
    v->vol = (uint8_t)((c->regs[0] >> 4) & 0x0Fu);
    v->duty = 2; /* 50% for bring-up smoke */
    v->period = (uint16_t)((c->regs[1] | ((c->regs[2] & 0x07u) << 8)) & 0x7FFu);
}

static void apu_reset_voices(R01sAtmega328p *c) {
    int i;
    for (i = 0; i < R01S_APU_CH_N; i++) {
        memset(&c->voice[i], 0, sizeof(c->voice[i]));
        c->voice[i].wave = APU_DEFAULT_WAVE[i];
        c->voice[i].duty = 2;
        c->voice[i].noise_lfsr = 1u;
    }
    c->mix = 128;
    c->scope_w = 0;
    memset(c->scope, 128, sizeof(c->scope));
}

static void apu_reset(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e->impl;
    memset(c->regs, 0, sizeof(c->regs));
    apu_reset_voices(c);
    c->pwm_hi_samples = 0;
    c->pwm_edges = 0;
    c->pwm_prev = R01S_LVL_L;
    r01s_bus_hiz(e, "DQ", 8);
    apu_drive_pwm(e, 0);
}

static void apu_eval(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e->impl;
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
    int i;
    int32_t mix = 0;
    int active = 0;
    int hi;
    R01sLevel out;

    sync_voice0_from_regs(c);

    for (i = 0; i < R01S_APU_CH_N; i++) {
        int16_t s = voice_sample(&c->voice[i]);
        if (c->voice[i].enable && c->voice[i].period >= 2u) {
            mix += s;
            active++;
        }
    }
    if (active > 0) {
        mix /= active;
    }
    if (mix < -128) {
        mix = -128;
    }
    if (mix > 127) {
        mix = 127;
    }
    c->mix = (uint8_t)(mix + 128);
    c->scope[c->scope_w % R01S_APU_SCOPE_N] = c->mix;
    c->scope_w++;

    /* PWM pin: voice-0 pulse (bring-up / health). */
    hi = 0;
    if (c->voice[0].enable && c->voice[0].period >= 2u && c->voice[0].vol > 0) {
        uint16_t thr = pulse_threshold(c->voice[0].period, c->voice[0].duty);
        hi = (c->voice[0].phase < thr);
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
    R01sAtmega328p *c = (R01sAtmega328p *)e->impl;
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

    /* Simplified APU port (not full AVR PDIP map -- decode TBD on schematic). */
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
    r01s_entity_set_dip_mm(&chip->base, 28, 35, 8);
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

const R01sApuVoice *r01s_atmega328p_voice(const R01sAtmega328p *chip, int ch) {
    if (!chip || ch < 0 || ch >= R01S_APU_CH_N) {
        return NULL;
    }
    return &chip->voice[ch];
}

uint8_t r01s_atmega328p_mix(const R01sAtmega328p *chip) {
    return chip ? chip->mix : 128;
}

int r01s_atmega328p_scope_copy(const R01sAtmega328p *chip, uint8_t *dst, int max_n) {
    int n;
    int i;
    unsigned start;
    if (!chip || !dst || max_n < 1) {
        return 0;
    }
    n = max_n;
    if (n > R01S_APU_SCOPE_N) {
        n = R01S_APU_SCOPE_N;
    }
    if (chip->scope_w < (unsigned)n) {
        n = (int)chip->scope_w;
    }
    start = chip->scope_w - (unsigned)n;
    for (i = 0; i < n; i++) {
        dst[i] = chip->scope[(start + (unsigned)i) % R01S_APU_SCOPE_N];
    }
    return n;
}

void r01s_atmega328p_voice_set(R01sAtmega328p *chip, int ch, uint8_t wave, uint8_t enable, uint8_t vol,
                               uint8_t duty, uint16_t period) {
    R01sApuVoice *v;
    if (!chip || ch < 0 || ch >= R01S_APU_CH_N) {
        return;
    }
    v = &chip->voice[ch];
    v->wave = (uint8_t)(wave & 3u);
    v->enable = enable ? 1u : 0u;
    v->vol = (uint8_t)(vol & 0x0Fu);
    v->duty = (uint8_t)(duty & 3u);
    v->period = (uint16_t)(period & 0x7FFu);
    if (ch == 0) {
        /* Mirror into legacy $FE40-$FE42 so bus smoke and voice stay aligned. */
        chip->regs[0] = (uint8_t)((v->enable ? 1u : 0u) | ((v->vol & 0x0Fu) << 4));
        chip->regs[1] = (uint8_t)(v->period & 0xFFu);
        chip->regs[2] = (uint8_t)((v->period >> 8) & 0x07u);
    }
}

int r01s_apu_voice_wave_y(const R01sApuVoice *v, int x, int width) {
    uint16_t period;
    uint16_t ph;
    int w = width > 0 ? width : 1;
    if (!v || !v->enable || v->period < 2u) {
        return 0;
    }
    period = v->period;
    /* Map display x across ~2 periods for readable shapes. */
    ph = (uint16_t)(((uint32_t)(x < 0 ? 0 : x) * period * 2u / (uint32_t)w) % period);

    switch (v->wave) {
    case R01S_APU_WAVE_TRIANGLE: {
        uint16_t half = period / 2u;
        int32_t y;
        if (half < 1u) {
            half = 1u;
        }
        if (ph < half) {
            y = ((int32_t)ph * 255) / (int32_t)half - 128;
        } else {
            y = 127 - (((int32_t)(ph - half) * 255) / (int32_t)half);
        }
        return (int)y;
    }
    case R01S_APU_WAVE_NOISE: {
        /* Deterministic hash from phase for a stable noise draw. */
        uint32_t h = (uint32_t)ph * 2654435761u;
        int bit = (int)((h >> 16) & 1u);
        int16_t amp = bit ? 127 : -128;
        if (v->vol == 0) {
            return 0;
        }
        return (int)((amp * (int16_t)v->vol) / 15);
    }
    case R01S_APU_WAVE_DPCM: {
        /* Stepped staircase over the period. */
        int steps = 8;
        int step = (int)((ph * (uint16_t)steps) / period);
        int16_t amp = (int16_t)(-96 + step * 28);
        if (v->vol == 0) {
            return 0;
        }
        return (int)((amp * (int16_t)v->vol) / 15);
    }
    case R01S_APU_WAVE_PULSE:
    default: {
        uint16_t thr = pulse_threshold(period, v->duty);
        int hi = (ph < thr);
        int16_t amp;
        if (v->vol == 0) {
            return 0;
        }
        amp = hi ? (int16_t)((127 * (int16_t)v->vol) / 15) : (int16_t)((-128 * (int16_t)v->vol) / 15);
        return (int)amp;
    }
    }
}
