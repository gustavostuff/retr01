#include "r01_apu_mix.h"

#include <math.h>
#include <string.h>

#define R01_APU_WT_N 64
#define R01_APU_WT_CH 3u

/*
 * Single-cycle tables, 64 points. Average of every 4 samples from Adventure Kid
 * Teensy 256-point dumps. CC0 1.0.
 * https://www.adventurekid.se/akrt/waveforms/adventure-kid-waveforms/
 */
static const int16_t k_wt[R01_APU_INS_COUNT][R01_APU_WT_N] = {
    /* Guitar: AKWF_aguitar_0001 */
    {
        5720,  17806,  27142,  31393,  32316,  32086,  30701,  27302,
        21232,  12426,   1750,  -8394, -15452, -19668, -22911, -25673,
       -27944, -29810, -30392, -29363, -28084, -26864, -24389, -20217,
       -14998,  -9046,  -3319,   2083,   8503,  15134,  19938,  23888,
        27606,  28693,  27144,  24982,  20416,  12522,   6843,   5102,
          543,  -7811, -13152, -14032, -14202, -12940,  -8595,  -4314,
        -2300,   -590,   1044,   1347,   2106,   4927,   6600,   2890,
        -4951, -11162, -12332, -10146,  -8322,  -8362,  -8112,  -3887,
    },
    /* EGuitar: AKWF_eguitar_0001 */
    {
        24534,  26922,  26882,  25861,  -1547,  -8899,   9727,  26035,
       -10403, -29616, -29480, -28950,   4018,  26894,  26727,  26616,
        26317,  18885,   1550,  12965,  21034, -30505, -30163, -29997,
       -20965,   6576,   -100, -22897, -25219,  26262,  26734,  26582,
        26313,   9080,   2715,  19004,  26066,  25890,  25624,  25464,
        25322,  25122,  24916,  24676,  24331,  22988,  20908,  15146,
       -17616, -32158, -32207, -32027, -31838, -21151, -25534, -31389,
       -31189, -30936, -30678, -30408, -30042, -29816, -29528, -28735,
    },
    /* Piano: AKWF_piano_0001 */
    {
         3725,   8084,   9131,  11982,   9058,  14331,  29549,  24410,
        12258,   1998,   3803,  18736,  16770,   3763,  -8968, -20728,
       -22059, -17704, -10899,   5627,  13352,   9848,  12029,  19653,
        24186,  13109,   6613,  10452,  11156,   4983,   -559,  -2755,
        -1516,  -6394, -18827, -17755, -14594, -15188, -14065,    659,
         5649,  -3294, -11447,  -1610,  14890,  17544,  12113,  14502,
        20208,  15821,   3604,  -5912, -10310, -20708, -28727, -30880,
       -16098,  -8234, -19990, -25056, -22567, -15964,  -5375,  -2822,
    },
    /* Flute: AKWF_flute_0001 */
    {
         1516,   3842,   5255,   5840,   6270,   7125,   8982,  10650,
        11699,  12261,  12853,  13684,  15179,  16885,  19344,  21774,
        23908,  25879,  28258,  30244,  31888,  32277,  31160,  29199,
        28032,  26922,  25288,  21950,  16729,  11232,   6082,    789,
        -3285,  -5428,  -5985,  -6613,  -7676,  -9638, -12051, -13341,
       -13822, -14688, -15750, -17003, -18090, -19252, -20965, -23388,
       -25807, -28385, -29455, -29503, -30150, -31162, -31426, -30416,
       -27650, -24500, -21758, -19593, -16756, -12486,  -7245,  -1969,
    },
};

static const double k_ins_decay_s[R01_APU_INS_COUNT] = {0.38, 0.34, 0.62, 1.55};

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
    double sr;
    if (!m) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->sample_rate = sample_rate > 0 ? sample_rate : 44100;
    sr = (double)m->sample_rate;
    for (ch = 0; ch < R01_APU_INS_COUNT; ch++) {
        m->env_mul[ch] = exp(-1.0 / (sr * k_ins_decay_s[ch]));
    }
    for (ch = 0; ch < R01_APU_CH_N; ch++) {
        m->v[ch].lfsr = 1u;
        m->v[ch].dpcm_acc = 64;
        m->ins[ch] = 0;
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
        uint16_t per = r01_apu_ch_period(m->regs, ch);
        if (wave == R01_APU_WAVE_DPCM) {
            uint8_t id = r01_apu_ch_dpcm_id(m->regs, ch);
            if (en && (!m->v[ch].last_en || id != m->v[ch].last_dpcm_id)) {
                dpcm_trigger(m, ch, id);
            }
            if (!en) {
                m->v[ch].dpcm_bits_left = 0;
            }
        } else if (ch < R01_APU_WT_CH) {
            if (en && (!m->v[ch].last_en || per != m->v[ch].last_per)) {
                m->v[ch].env = 1.0;
                m->v[ch].phase = 0.0;
            }
            if (!en) {
                m->v[ch].env = 0.0;
            }
            m->v[ch].last_per = per;
        }
        m->v[ch].last_en = en;
    }
}

void r01_apu_mix_set_ins(R01ApuMix *m, const uint8_t ins[R01_APU_CH_N]) {
    uint8_t ch;
    if (!m) {
        return;
    }
    for (ch = 0; ch < R01_APU_CH_N; ch++) {
        uint8_t v = ins ? ins[ch] : 0u;
        if (v >= R01_APU_INS_COUNT) {
            v = 0u;
        }
        m->ins[ch] = v;
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
    if (ch < R01_APU_WT_CH) {
        double x = v->phase * (double)R01_APU_WT_N;
        int i0 = (int)x;
        double frac = x - (double)i0;
        int i1;
        double s;
        int ins = (int)m->ins[ch];
        if (ins < 0 || ins >= R01_APU_INS_COUNT) {
            ins = 0;
        }
        i0 &= (R01_APU_WT_N - 1);
        i1 = (i0 + 1) & (R01_APU_WT_N - 1);
        s = (double)k_wt[ins][i0] + ((double)k_wt[ins][i1] - (double)k_wt[ins][i0]) * frac;
        s *= v->env * ((double)vol / 15.0) / 16.0;
        v->env *= m->env_mul[ins];
        if (v->env < 1.0e-4) {
            v->env = 0.0;
        }
        if (s > 32767.0) {
            s = 32767.0;
        }
        if (s < -32768.0) {
            s = -32768.0;
        }
        return (int16_t)s;
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
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[i] = (int16_t)mix;
    }
}
