#include "r01_nes_synth.h"

#include <ctype.h>
#include <math.h>
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

/* NES-ish noise period table (approx), indexed by low nibble. */
static const float k_noise_hz[16] = {
    4470.f, 2240.f, 1120.f, 560.f, 280.f, 186.f, 140.f, 111.f,
    88.f,   70.f,   59.f,   47.f,  37.f,  31.f,  23.f,  18.f,
};

void r01_nes_synth_init(R01NesSynth *s, int sample_rate) {
    int i;
    if (!s) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 0 ? sample_rate : 44100;
    for (i = 0; i < R01_NES_CH_N; i++) {
        s->v[i].noise_lfsr = 1;
        s->v[i].vol = 10;
        s->v[i].duty = 2;
    }
    s->v[0].wave = R01_NES_WAVE_PULSE;
    s->v[1].wave = R01_NES_WAVE_PULSE;
    s->v[2].wave = R01_NES_WAVE_TRIANGLE;
    s->v[3].wave = R01_NES_WAVE_NOISE;
    s->v[4].wave = R01_NES_WAVE_DPCM;
}

void r01_nes_synth_silence(R01NesSynth *s) {
    int i;
    if (!s) {
        return;
    }
    for (i = 0; i < R01_NES_CH_N; i++) {
        s->v[i].enable = 0;
        s->v[i].freq_hz = 0.f;
        s->v[i].dpcm_samples_left = 0;
    }
}

void r01_nes_synth_off(R01NesSynth *s, int ch) {
    if (!s || ch < 0 || ch >= R01_NES_CH_N) {
        return;
    }
    s->v[ch].enable = 0;
    s->v[ch].freq_hz = 0.f;
    if (ch == 4) {
        s->v[ch].dpcm_samples_left = 0;
    }
}

void r01_nes_synth_set_pulse(R01NesSynth *s, int ch, float freq_hz, int vol, int duty) {
    R01NesVoice *v;
    if (!s || (ch != 0 && ch != 1)) {
        return;
    }
    v = &s->v[ch];
    v->wave = R01_NES_WAVE_PULSE;
    v->duty = duty & 3;
    v->vol = vol < 0 ? 0 : (vol > 15 ? 15 : vol);
    if (freq_hz < 20.f) {
        v->enable = 0;
        v->freq_hz = 0.f;
        return;
    }
    v->freq_hz = freq_hz;
    v->enable = 1;
}

void r01_nes_synth_set_triangle(R01NesSynth *s, float freq_hz) {
    R01NesVoice *v;
    if (!s) {
        return;
    }
    v = &s->v[2];
    v->wave = R01_NES_WAVE_TRIANGLE;
    v->vol = 15;
    if (freq_hz < 20.f) {
        v->enable = 0;
        v->freq_hz = 0.f;
        return;
    }
    v->freq_hz = freq_hz;
    v->enable = 1;
}

void r01_nes_synth_set_noise(R01NesSynth *s, int period_code, int vol) {
    R01NesVoice *v;
    if (!s) {
        return;
    }
    v = &s->v[3];
    v->wave = R01_NES_WAVE_NOISE;
    v->vol = vol < 0 ? 0 : (vol > 15 ? 15 : vol);
    v->freq_hz = k_noise_hz[period_code & 15];
    v->enable = v->vol > 0;
}

void r01_nes_synth_trigger_dpcm(R01NesSynth *s, int kind) {
    R01NesVoice *v;
    if (!s) {
        return;
    }
    v = &s->v[4];
    v->wave = R01_NES_WAVE_DPCM;
    v->enable = 1;
    v->vol = 12;
    v->dpcm_acc = 0;
    /* Short one-shot: kick ~40ms, hit ~20ms at 44.1k. */
    v->dpcm_samples_left = (kind == 0) ? (s->sample_rate / 25) : (s->sample_rate / 50);
    v->freq_hz = (kind == 0) ? 80.f : 160.f;
}

static int16_t voice_sample(R01NesSynth *s, R01NesVoice *v) {
    double step;
    int16_t amp = 0;
    if (!v->enable) {
        return 0;
    }
    if (v->wave == R01_NES_WAVE_DPCM) {
        float t;
        if (v->dpcm_samples_left <= 0) {
            v->enable = 0;
            return 0;
        }
        v->dpcm_samples_left--;
        /* Crude decaying triangle burst as DPCM stand-in. */
        step = (double)v->freq_hz / (double)s->sample_rate;
        v->phase += step;
        if (v->phase >= 1.0) {
            v->phase -= 1.0;
        }
        t = (float)((v->phase < 0.5) ? (v->phase * 4.0 - 1.0) : (3.0 - v->phase * 4.0));
        amp = (int16_t)(t * 8000.f * ((float)v->dpcm_samples_left / (float)(s->sample_rate / 25 + 1)));
        return amp;
    }
    if (v->freq_hz < 1.f) {
        return 0;
    }
    step = (double)v->freq_hz / (double)s->sample_rate;
    v->phase += step;
    while (v->phase >= 1.0) {
        v->phase -= 1.0;
        if (v->wave == R01_NES_WAVE_NOISE) {
            uint16_t l = v->noise_lfsr ? v->noise_lfsr : 1u;
            uint16_t bit = (uint16_t)(((l >> 0) ^ (l >> 1)) & 1u);
            v->noise_lfsr = (uint16_t)((l >> 1) | (bit << 14));
        }
    }
    switch (v->wave) {
    case R01_NES_WAVE_PULSE: {
        float d = duty_frac(v->duty);
        amp = (v->phase < (double)d) ? 1 : -1;
        amp = (int16_t)((amp * 2000 * v->vol) / 15);
        break;
    }
    case R01_NES_WAVE_TRIANGLE: {
        float t = (float)((v->phase < 0.5) ? (v->phase * 4.0 - 1.0) : (3.0 - v->phase * 4.0));
        amp = (int16_t)(t * 2800.f); /* full amp, no volume */
        break;
    }
    case R01_NES_WAVE_NOISE: {
        amp = (v->noise_lfsr & 1u) ? 1 : -1;
        amp = (int16_t)((amp * 1600 * v->vol) / 15);
        break;
    }
    default:
        amp = 0;
        break;
    }
    return amp;
}

void r01_nes_synth_render(R01NesSynth *s, int16_t *out, int frames) {
    int i, ch;
    if (!s || !out || frames < 1) {
        return;
    }
    for (i = 0; i < frames; i++) {
        int32_t mix = 0;
        for (ch = 0; ch < R01_NES_CH_N; ch++) {
            mix += voice_sample(s, &s->v[ch]);
        }
        /* Fixed headroom so solo and multi keep the same per-voice level. */
        mix /= R01_NES_CH_N;
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[i] = (int16_t)mix;
    }
}

int r01_nes_parse_hex_u8(const char *tok, int *out_v) {
    int v = 0;
    int i;
    if (!tok || !tok[0] || !tok[1] || tok[2]) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        char c = (char)toupper((unsigned char)tok[i]);
        int n;
        if (c >= '0' && c <= '9') {
            n = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            n = 10 + (c - 'A');
        } else {
            return 0;
        }
        v = (v << 4) | n;
    }
    if (out_v) {
        *out_v = v;
    }
    return 1;
}

int r01_nes_parse_note_hz(const char *tok, float *out_hz) {
    static const int letter_pc[8] = {/* G A B C D E F placeholder index by letter */
                                     0};
    int pc = -1;
    int octave = 4;
    int sharp = 0;
    int i = 0;
    int midi;
    float hz;
    char L;
    (void)letter_pc;
    if (!tok || !tok[0] || (tok[0] == '-' && tok[1] == '-')) {
        if (out_hz) {
            *out_hz = 0.f;
        }
        return 0;
    }
    L = (char)toupper((unsigned char)tok[0]);
    switch (L) {
    case 'C':
        pc = 0;
        break;
    case 'D':
        pc = 2;
        break;
    case 'E':
        pc = 4;
        break;
    case 'F':
        pc = 5;
        break;
    case 'G':
        pc = 7;
        break;
    case 'A':
        pc = 9;
        break;
    case 'B':
        pc = 11;
        break;
    default:
        return 0;
    }
    i = 1;
    if (tok[i] == '#' || tok[i] == 's' || tok[i] == 'S') {
        sharp = 1;
        i++;
    } else if (tok[i] == 'b' || tok[i] == 'B') {
        /* Ambiguous: "Bb4" vs note B + junk. Prefer flat when next is digit. */
        if (tok[i + 1] && isdigit((unsigned char)tok[i + 1])) {
            sharp = -1;
            i++;
        }
    }
    if (!tok[i] || !isdigit((unsigned char)tok[i])) {
        return 0;
    }
    octave = tok[i] - '0';
    if (tok[i + 1]) {
        return 0; /* only single-digit octave */
    }
    midi = (octave + 1) * 12 + pc + sharp;
    hz = 440.f * powf(2.f, ((float)midi - 69.f) / 12.f);
    if (out_hz) {
        *out_hz = hz;
    }
    return 1;
}
