#ifndef retr01_SIM_ATMEGA328P_H
#define retr01_SIM_ATMEGA328P_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_APU_REGS 0x20u
#define R01S_APU_CH_N 8
#define R01S_APU_BGM_N 5 /* channels 0-4 */
#define R01S_APU_SFX_N 3 /* channels 5-7 */
#define R01S_APU_SCOPE_N 160

/* Waveforms match docs/sound.md channel map. */
typedef enum R01sApuWave {
    R01S_APU_WAVE_PULSE = 0,
    R01S_APU_WAVE_TRIANGLE = 1,
    R01S_APU_WAVE_NOISE = 2,
    R01S_APU_WAVE_DPCM = 3,
} R01sApuWave;

typedef struct R01sApuVoice {
    uint8_t wave;   /* R01sApuWave */
    uint8_t enable; /* 1 = sounding */
    uint8_t vol;    /* 0-15 (triangle ignores; always full when on) */
    uint8_t duty;   /* pulse: 0=12.5% 1=25% 2=50% 3=75% */
    uint16_t period;
    uint16_t phase;
    uint16_t noise_lfsr;
    int8_t dpcm_acc; /* 7-bit-ish accumulator for DPCM stub */
    int16_t sample;  /* last digital sample -128..127 */
} R01sApuVoice;

/*
 * Island K -- ATmega328P APU stub (behavioral; not a full AVR core).
 *
 * Soft bus window $FE40-$FE5F (A0-A4). Legacy smoke map (channel 1 / voice 0):
 *   [0] $FE40  bit0=enable, bits4-7=volume (0-15)
 *   [1] $FE41  period low
 *   [2] $FE42  period high (bits0-2) -> 11-bit period
 *   [3..]      storage / future protocol
 *
 * Eight software voices (BGM 1-5 + SFX 6-8) mix to an 8-bit analog sample
 * (R-2R stand-in). PWM pin still tracks voice-0 pulse for bring-up health.
 */
typedef struct R01sAtmega328p {
    R01sEntity base;
    uint8_t regs[R01S_APU_REGS];
    R01sApuVoice voice[R01S_APU_CH_N];
    uint8_t mix; /* 0-255 analog mix (center ~128) */
    uint8_t scope[R01S_APU_SCOPE_N];
    unsigned scope_w;
    uint32_t pwm_hi_samples;
    uint32_t pwm_edges;
    R01sLevel pwm_prev;
} R01sAtmega328p;

void r01s_atmega328p_init(R01sAtmega328p *chip, const char *refdes);
R01sEntity *r01s_atmega328p_entity(R01sAtmega328p *chip);

uint8_t r01s_atmega328p_peek(const R01sAtmega328p *chip, unsigned reg);
void r01s_atmega328p_poke(R01sAtmega328p *chip, unsigned reg, uint8_t data);

int r01s_atmega328p_enabled(const R01sAtmega328p *chip);
uint16_t r01s_atmega328p_period(const R01sAtmega328p *chip);
uint32_t r01s_atmega328p_pwm_edges(const R01sAtmega328p *chip);
uint32_t r01s_atmega328p_pwm_hi_samples(const R01sAtmega328p *chip);

const R01sApuVoice *r01s_atmega328p_voice(const R01sAtmega328p *chip, int ch);
uint8_t r01s_atmega328p_mix(const R01sAtmega328p *chip);
/* Copy newest scope samples (analog mix history). Returns count written. */
int r01s_atmega328p_scope_copy(const R01sAtmega328p *chip, uint8_t *dst, int max_n);

/*
 * Configure a voice for tests / future sequencer (does not write $FE4x).
 * wave = R01sApuWave; duty 0-3; vol 0-15; period >= 2 to run.
 */
void r01s_atmega328p_voice_set(R01sAtmega328p *chip, int ch, uint8_t wave, uint8_t enable, uint8_t vol,
                               uint8_t duty, uint16_t period);

/*
 * Ideal waveform amplitude -128..127 for display column x in [0, width).
 * Uses voice period/duty/wave; independent of scope ring (math draw).
 */
int r01s_apu_voice_wave_y(const R01sApuVoice *v, int x, int width);

#endif
