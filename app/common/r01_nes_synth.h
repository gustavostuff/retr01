#ifndef R01_NES_SYNTH_H
#define R01_NES_SYNTH_H

#include <stdint.h>

/* Compact NES 2A03-style softsynth (pulse x2, triangle, noise, DPCM stub).
 * Public-domain style first-party code aligned with Retr01 sim APU voices —
 * not a full cycle-accurate APU. */

#define R01_NES_CH_N 5

typedef enum R01NesWave {
    R01_NES_WAVE_PULSE = 0,
    R01_NES_WAVE_TRIANGLE = 1,
    R01_NES_WAVE_NOISE = 2,
    R01_NES_WAVE_DPCM = 3
} R01NesWave;

typedef struct R01NesVoice {
    int enable;
    R01NesWave wave;
    float freq_hz;
    int duty; /* pulse 0..3 -> 12.5/25/50/75% */
    int vol;  /* 0..15 (triangle ignores; full when on) */
    double phase; /* 0..1 */
    uint16_t noise_lfsr;
    int dpcm_samples_left;
    int8_t dpcm_acc;
} R01NesVoice;

typedef struct R01NesSynth {
    int sample_rate;
    R01NesVoice v[R01_NES_CH_N];
} R01NesSynth;

void r01_nes_synth_init(R01NesSynth *s, int sample_rate);
void r01_nes_synth_silence(R01NesSynth *s);
void r01_nes_synth_set_pulse(R01NesSynth *s, int ch, float freq_hz, int vol, int duty);
void r01_nes_synth_set_triangle(R01NesSynth *s, float freq_hz);
void r01_nes_synth_set_noise(R01NesSynth *s, int period_code, int vol);
void r01_nes_synth_trigger_dpcm(R01NesSynth *s, int kind); /* 0=kick stub, 1=hit */
void r01_nes_synth_off(R01NesSynth *s, int ch);
void r01_nes_synth_render(R01NesSynth *s, int16_t *out, int frames);

/* Parse Studio tracker tokens: "--", "C4", "Eb3", "8F" (noise), "FD" (DPCM). */
int r01_nes_parse_note_hz(const char *tok, float *out_hz);
int r01_nes_parse_hex_u8(const char *tok, int *out_v);

#endif
