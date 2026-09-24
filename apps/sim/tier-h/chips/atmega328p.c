#include "atmega328p.h"

#include "r01_apu_tracker.h"
#include "r01_bgm_fd.h"
#include "r01_nes_synth.h"
#include "retr01_sim/bus.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    if (r01s_entity_pin_named(e, "AUDIO_PWM")) {
        r01s_entity_drive(e, "AUDIO_PWM", hi ? R01S_LVL_H : R01S_LVL_L);
    } else {
        r01s_entity_drive(e, "PWM", hi ? R01S_LVL_H : R01S_LVL_L);
    }
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

static void sync_voices_from_regs(R01sAtmega328p *c) {
    int ch;
    for (ch = 0; ch < R01S_APU_CH_N; ch++) {
        R01sApuVoice *v = &c->voice[ch];
        unsigned b = (unsigned)ch * 4u;
        v->enable = (c->regs[b] & 0x01u) != 0;
        v->vol = (uint8_t)((c->regs[b] >> 4) & 0x0Fu);
        v->period = (uint16_t)((c->regs[b + 1u] | ((c->regs[b + 2u] & 0x07u) << 8)) & 0x7FFu);
        v->duty = (uint8_t)((c->regs[b + 2u] >> 4) & 0x03u);
        v->wave = (uint8_t)(c->regs[b + 3u] & 0x03u);
    }
}

static void sync_voice0_from_regs(R01sAtmega328p *c) {
    /* Legacy name: full window sync (8x4 packing matches hw/firmware/common/r01_apu_window.h). */
    sync_voices_from_regs(c);
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
    memset(&c->viz, 0, sizeof(c->viz));
}

static void apu_reset(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e->impl;
    memset(c->regs, 0, sizeof(c->regs));
    apu_reset_voices(c);
    c->pwm_hi_samples = 0;
    c->pwm_edges = 0;
    c->pwm_prev = R01S_LVL_L;
    if (r01s_entity_pin_named(e, "DQ0")) {
        r01s_bus_hiz(e, "DQ", 8);
    }
    apu_drive_pwm(e, 0);
}

static void apu_eval(R01sEntity *e) {
    R01sAtmega328p *c = (R01sAtmega328p *)e->impl;
    if (!r01s_entity_pin_named(e, "CE#")) {
        return; /* mailbox-owned S2 shell */
    }
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
        /* Mirror into legacy $7F40-$7F42 so bus smoke and voice stay aligned. */
        chip->regs[0] = (uint8_t)((v->enable ? 1u : 0u) | ((v->vol & 0x0Fu) << 4));
        chip->regs[1] = (uint8_t)(v->period & 0xFFu);
        chip->regs[2] = (uint8_t)(((v->period >> 8) & 0x07u) | ((v->duty & 0x03u) << 4));
        chip->regs[3] = (uint8_t)(v->wave & 0x03u);
    } else if (ch > 0 && ch < R01S_APU_CH_N) {
        unsigned b = (unsigned)ch * 4u;
        chip->regs[b] = (uint8_t)((v->enable ? 1u : 0u) | ((v->vol & 0x0Fu) << 4));
        chip->regs[b + 1u] = (uint8_t)(v->period & 0xFFu);
        chip->regs[b + 2u] = (uint8_t)(((v->period >> 8) & 0x07u) | ((v->duty & 0x03u) << 4));
        chip->regs[b + 3u] = (uint8_t)(v->wave & 0x03u);
    }
}

int r01s_apu_voice_wave_y(const R01sApuVoice *v, int x, int width) {
    uint16_t period;
    uint16_t ph;
    int w = width > 0 ? width : 1;
    /* Synth ticks spanned by the lane. Match typical WAVE lane width so 1px≈1 tick. */
    const uint32_t view_ticks = 96u;
    uint32_t t;
    if (!v || !v->enable || v->period < 2u) {
        return 0;
    }
    period = v->period;
    /* Fixed time window: higher pitch => more cycles across the lane. */
    t = ((uint32_t)(x < 0 ? 0 : x) * view_ticks) / (uint32_t)w;
    ph = (uint16_t)((t + (uint32_t)v->phase) % (uint32_t)period);

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

static void viz_voice_off(R01sAtmega328p *chip, int ch) {
    uint8_t wave = APU_DEFAULT_WAVE[ch];
    r01s_atmega328p_voice_set(chip, ch, wave, 0, 0, 2, 0);
}

static void viz_tracker_tick(R01sAtmega328p *chip) {
    R01sApuViz *vz;
    if (!chip || !chip->viz.active) {
        return;
    }
    vz = &chip->viz;
    (void)r01_apu_tracker_nmi(&vz->tracker, chip->regs);
    sync_voices_from_regs(chip);
    vz->fe4x_dirty = 1;
    vz->nmi_i++;
    if (vz->frames_per_step > 0 && vz->track_steps > 0 && vz->nmi_i > 0) {
        vz->step = ((vz->nmi_i - 1) / vz->frames_per_step) % vz->track_steps;
    }
}

static void viz_tracker_advance(R01sAtmega328p *chip) {
    viz_tracker_tick(chip);
}

static void viz_load_builtin_track1(R01sApuViz *vz) {
    /* Fallback when no Studio export bin is present. */
    static const char *demo[][R01S_APU_BGM_N] = {
        {"C4", "--", "--", "--", "--"}, {"C4", "--", "--", "--", "--"}, {"--", "F4", "--", "--", "--"},
        {"--", "F4", "--", "--", "--"}, {"--", "--", "B3", "--", "--"}, {"--", "--", "G3", "--", "--"},
        {"--", "--", "--", "80", "--"}, {"--", "--", "--", "80", "--"}, {"--", "--", "--", "80", "--"},
        {"--", "--", "--", "--", "--"},
    };
    int r, c;
    memset(vz->cell, 0, sizeof(vz->cell));
    vz->track_steps = (int)(sizeof(demo) / sizeof(demo[0]));
    if (vz->track_steps > R01S_APU_VIZ_STEPS_MAX) {
        vz->track_steps = R01S_APU_VIZ_STEPS_MAX;
    }
    for (r = 0; r < R01S_APU_VIZ_STEPS_MAX; r++) {
        for (c = 0; c < R01S_APU_BGM_N; c++) {
            snprintf(vz->cell[r][c], R01S_APU_VIZ_TOKEN, "--");
        }
    }
    for (r = 0; r < vz->track_steps; r++) {
        for (c = 0; c < R01S_APU_BGM_N; c++) {
            snprintf(vz->cell[r][c], R01S_APU_VIZ_TOKEN, "%s", demo[r][c]);
        }
    }
}

/* Flat bin from Studio export / Host Play: steps * BGM_CH * TOKEN bytes. */
static int viz_load_bin(R01sApuViz *vz, const char *path) {
    FILE *f;
    long sz;
    int steps;
    int t, ch;
    unsigned char *buf = NULL;
    size_t need;
    if (!vz || !path || !path[0]) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    need = (size_t)R01S_APU_BGM_N * (size_t)R01S_APU_VIZ_TOKEN;
    if (sz < (long)need || ((size_t)sz % need) != 0) {
        fclose(f);
        return -1;
    }
    steps = (int)((size_t)sz / need);
    if (steps < 1) {
        fclose(f);
        return -1;
    }
    if (steps > R01S_APU_VIZ_STEPS_MAX) {
        steps = R01S_APU_VIZ_STEPS_MAX;
    }
    buf = (unsigned char *)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    memset(vz->cell, 0, sizeof(vz->cell));
    for (t = 0; t < steps; t++) {
        for (ch = 0; ch < R01S_APU_BGM_N; ch++) {
            size_t off = ((size_t)t * (size_t)R01S_APU_BGM_N + (size_t)ch) * (size_t)R01S_APU_VIZ_TOKEN;
            char tok[R01S_APU_VIZ_TOKEN];
            int i;
            memcpy(tok, buf + off, (size_t)R01S_APU_VIZ_TOKEN);
            tok[R01S_APU_VIZ_TOKEN - 1] = '\0';
            for (i = 0; i < R01S_APU_VIZ_TOKEN; i++) {
                if (tok[i] == '\0') {
                    break;
                }
            }
            if (i == 0) {
                snprintf(vz->cell[t][ch], R01S_APU_VIZ_TOKEN, "--");
            } else {
                snprintf(vz->cell[t][ch], R01S_APU_VIZ_TOKEN, "%s", tok);
            }
        }
    }
    free(buf);
    vz->track_steps = steps;
    return 0;
}

void r01s_atmega328p_viz_start(R01sAtmega328p *chip, uint32_t now_ms, const char *bgm_bin_path) {
    R01sApuViz *vz;
    int ch;
    int n;
    if (!chip) {
        return;
    }
    vz = &chip->viz;
    memset(vz, 0, sizeof(*vz));
    if (viz_load_bin(vz, bgm_bin_path) != 0) {
        viz_load_builtin_track1(vz);
    }
    n = r01_bgm_fd_encode_cells((const char(*)[R01_BGM_FD_CH][R01_BGM_FD_TOKEN])vz->cell, vz->track_steps,
                                vz->bytecode, (unsigned)sizeof(vz->bytecode));
    if (n < 0) {
        static const uint8_t demo[] = {R01_APU_FD_OP, 0x01u, 0xC4u, R01_APU_CTRL_FE, 0x01u, R01_APU_CTRL_FA};
        memcpy(vz->bytecode, demo, sizeof(demo));
        vz->bytecode_len = (uint16_t)sizeof(demo);
    } else {
        vz->bytecode_len = (uint16_t)n;
    }
    r01_apu_tracker_init(&vz->tracker);
    r01_apu_tracker_set_bgm(&vz->tracker, vz->bytecode, vz->bytecode_len);
    vz->frames_per_step = r01_bgm_fd_frames_per_step();
    if (vz->frames_per_step < 1) {
        vz->frames_per_step = 1;
    }
    /* Wall-clock NMI period; TEMPO_SCALE > 1 speeds the sequencer preview. */
    vz->ms_per_nmi = 1000 / (R01_BGM_FD_NMI_HZ * R01S_APU_VIZ_TEMPO_SCALE);
    if (vz->ms_per_nmi < 1) {
        vz->ms_per_nmi = 1;
    }
    vz->last_ms = now_ms;
    vz->step = 0;
    vz->nmi_i = 0;
    vz->ms_accum = 0;
    vz->active = 1;
    for (ch = 0; ch < R01S_APU_CH_N; ch++) {
        viz_voice_off(chip, ch);
    }
    viz_tracker_tick(chip);
}

void r01s_atmega328p_viz_stop(R01sAtmega328p *chip) {
    int ch;
    if (!chip) {
        return;
    }
    chip->viz.active = 0;
    for (ch = 0; ch < R01S_APU_CH_N; ch++) {
        viz_voice_off(chip, ch);
    }
}

void r01s_atmega328p_viz_frame(R01sAtmega328p *chip, uint32_t now_ms) {
    R01sApuViz *vz;
    uint32_t dt;
    int i;
    if (!chip || !chip->viz.active) {
        return;
    }
    vz = &chip->viz;
    if (now_ms < vz->last_ms) {
        vz->last_ms = now_ms;
    }
    dt = now_ms - vz->last_ms;
    vz->last_ms = now_ms;
    if (dt > 100u) {
        /* Pause / hitch: don't skip half the track. */
        dt = 100u;
    }
    vz->ms_accum += (int)dt;
    if (vz->dpcm_ms_left > 0) {
        vz->dpcm_ms_left -= (int)dt;
        if (vz->dpcm_ms_left <= 0) {
            vz->dpcm_ms_left = 0;
            viz_voice_off(chip, 4);
        }
    }
    while (vz->ms_accum >= vz->ms_per_nmi) {
        vz->ms_accum -= vz->ms_per_nmi;
        viz_tracker_advance(chip);
    }
    /* Extra synth ticks so MIX lane A scrolls even when board steps are few. */
    for (i = 0; i < R01S_APU_VIZ_SYNTH_PER_FRAME; i++) {
        apu_synth_tick(chip, &chip->base);
    }
}

int r01s_atmega328p_viz_active(const R01sAtmega328p *chip) {
    return chip && chip->viz.active;
}

int r01s_atmega328p_viz_step(const R01sAtmega328p *chip) {
    return chip && chip->viz.active ? chip->viz.step : -1;
}

int r01s_atmega328p_viz_fe4x_dirty(const R01sAtmega328p *chip) {
    return chip && chip->viz.fe4x_dirty;
}

void r01s_atmega328p_viz_clear_fe4x_dirty(R01sAtmega328p *chip) {
    if (chip) {
        chip->viz.fe4x_dirty = 0;
    }
}
