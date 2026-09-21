#include "r01_apu_mix.h"

#include <string.h>

static float duty_frac(int duty) {
    switch (duty & 3) {
    case 0:
        return 0.125f;
    case 1:
        return 0.25f;
    case 3:
        return 0.75f;
    case 2:
    default:
        return 0.5f;
    }
}

/* 64-bit stand-in streams (S2 flash on hardware). ID 1 kick-ish, 2 snare-ish. */
static const uint8_t k_dpcm[16][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x07, 0x00, 0x00},
    {0xAA, 0x55, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x00},
    {0xF0, 0xF0, 0x0F, 0x0F, 0xCC, 0x33, 0x80, 0x00},
    {0xFF, 0x00, 0xFF, 0x00, 0xF0, 0x00, 0x00, 0x00},
    {0xC0, 0x00, 0xC0, 0x00, 0x80, 0x00, 0x00, 0x00},
    {0xFE, 0x00, 0x7F, 0x00, 0x3E, 0x00, 0x00, 0x00},
    {0xAA, 0xAA, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00},
    {0xFF, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00},
    {0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00},
    {0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static const uint16_t k_dpcm_bits[16] = {
    8u, 56u, 48u, 40u, 32u, 24u, 40u, 32u, 16u, 20u, 24u, 16u, 16u, 32u, 32u, 12u,
};

static void dpcm_trigger(R01ApuMix *m, uint8_t ch, uint8_t id) {
    R01ApuMixVoice *v = &m->v[ch];
    v->dpcm_acc = 64;
    v->dpcm_bit_i = 0;
    v->dpcm_frac = 0;
    v->dpcm_bits_left = (int)k_dpcm_bits[id & 0x0Fu];
    v->last_dpcm_id = (uint8_t)(id & 0x0Fu);
}

void r01_apu_mix_init(R01ApuMix *m, int sample_rate) {
    uint8_t ch;
    if (!m) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->sample_rate = sample_rate > 0 ? sample_rate : 44100;
    for (ch = 0; ch < R01_APU_CH_N; ch++) {
        m->v[ch].lfsr = 1u;
        m->v[ch].dpcm_acc = 64;
    }
}

void r01_apu_mix_set_regs(R01ApuMix *m, const uint8_t *regs) {
    uint8_t ch;
    if (!m || !regs) {
        return;
    }
    memcpy(m->regs, regs, R01_APU_REGS);
    for (ch = 0; ch < R01_APU_CH_N; ch++) {
        uint8_t en = r01_apu_ch_enabled(m->regs, ch) ? 1u : 0u;
        uint8_t wave = r01_apu_ch_wave(m->regs, ch);
        if (wave == R01_APU_WAVE_DPCM) {
            uint8_t id = r01_apu_ch_dpcm_id(m->regs, ch);
            if (en && (!m->v[ch].last_en || id != m->v[ch].last_dpcm_id)) {
                dpcm_trigger(m, ch, id);
            }
            if (!en) {
                m->v[ch].dpcm_bits_left = 0;
            }
        }
        m->v[ch].last_en = en;
    }
}

static int dpcm_bit(uint8_t id, unsigned i) {
    unsigned byte = (i >> 3) & 7u;
    unsigned bit = i & 7u;
    return (k_dpcm[id & 0x0Fu][byte] >> bit) & 1;
}

static int16_t voice_sample(R01ApuMix *m, uint8_t ch) {
    R01ApuMixVoice *v = &m->v[ch];
    uint8_t wave;
    uint16_t per;
    float hz;
    double step;
    int16_t amp = 0;
    int vol;

    if (!r01_apu_ch_enabled(m->regs, ch)) {
        return 0;
    }
    wave = r01_apu_ch_wave(m->regs, ch);
    vol = (int)r01_apu_ch_vol(m->regs, ch);

    if (wave == R01_APU_WAVE_DPCM) {
        int acc;
        if (v->dpcm_bits_left <= 0 || m->sample_rate < 1) {
            return 0;
        }
        v->dpcm_frac += R01_APU_DPCM_HZ;
        while (v->dpcm_frac >= m->sample_rate && v->dpcm_bits_left > 0) {
            v->dpcm_frac -= m->sample_rate;
            if (dpcm_bit(v->last_dpcm_id, v->dpcm_bit_i)) {
                acc = (int)v->dpcm_acc + 2;
            } else {
                acc = (int)v->dpcm_acc - 2;
            }
            if (acc < 0) {
                acc = 0;
            }
            if (acc > 127) {
                acc = 127;
            }
            v->dpcm_acc = (int8_t)acc;
            v->dpcm_bit_i++;
            v->dpcm_bits_left--;
        }
        amp = (int16_t)(((int)v->dpcm_acc - 64) * 80);
        return amp;
    }

    per = r01_apu_ch_period(m->regs, ch);
    if (per < 2u) {
        return 0;
    }
    hz = (float)R01_APU_PERIOD_CLOCK / (float)per;
    if (hz < 1.f) {
        return 0;
    }
    step = (double)hz / (double)m->sample_rate;
    v->phase += step;
    while (v->phase >= 1.0) {
        v->phase -= 1.0;
        if (wave == R01_APU_WAVE_NOISE) {
            uint16_t l = v->lfsr ? v->lfsr : 1u;
            uint16_t bit = (uint16_t)(((l >> 0) ^ (l >> 1)) & 1u);
            v->lfsr = (uint16_t)((l >> 1) | (bit << 14));
        }
    }
    switch (wave) {
    case R01_APU_WAVE_PULSE: {
        float d = duty_frac((int)r01_apu_ch_duty(m->regs, ch));
        amp = (v->phase < (double)d) ? 1 : -1;
        amp = (int16_t)((amp * 2000 * vol) / 15);
        break;
    }
    case R01_APU_WAVE_TRIANGLE: {
        float t = (float)((v->phase < 0.5) ? (v->phase * 4.0 - 1.0) : (3.0 - v->phase * 4.0));
        amp = (int16_t)(t * 2800.f);
        break;
    }
    case R01_APU_WAVE_NOISE: {
        amp = (v->lfsr & 1u) ? 1 : -1;
        amp = (int16_t)((amp * 1600 * vol) / 15);
        break;
    }
    default:
        amp = 0;
        break;
    }
    return amp;
}

void r01_apu_mix_render(R01ApuMix *m, int16_t *out, int frames) {
    int i;
    uint8_t ch;
    if (!m || !out || frames < 1) {
        return;
    }
    for (i = 0; i < frames; i++) {
        int32_t mix = 0;
        for (ch = 0; ch < R01_APU_CH_N; ch++) {
            mix += voice_sample(m, ch);
        }
        mix /= 4;
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[i] = (int16_t)mix;
    }
}
