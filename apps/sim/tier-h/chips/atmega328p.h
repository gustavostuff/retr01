#ifndef retr01_SIM_ATMEGA328P_H
#define retr01_SIM_ATMEGA328P_H

#include "retr01_sim/entity.h"

#include "r01_apu_tracker.h"
#include "r01_bgm_fd.h"

#include <stdint.h>

#define R01S_APU_REGS 0x20u
#define R01S_APU_CH_N 8
#define R01S_APU_BGM_N 5 /* channels 0-4 */
#define R01S_APU_SFX_N 3 /* channels 5-7 */
#define R01S_APU_SCOPE_N 160
/* Silent WAVE-monitor BGM (no speaker). Prefer exported Track-1 bin when present.
 * Sequencer follows cart bytecode (FD + FE holds) at ~60 Hz NMI, same as emu.
 * TEMPO_SCALE speeds the NMI clock (1 = Studio tempo). IC board time is separately
 * budget-limited in app.c (R01S_SIM_BUDGET_MS*). */
#define R01S_APU_VIZ_TEMPO_BPM R01_BGM_FD_TEMPO_BPM
#define R01S_APU_VIZ_STEPS_PER_BEAT R01_BGM_FD_STEPS_PER_BEAT
#define R01S_APU_VIZ_TEMPO_SCALE 1
#define R01S_APU_VIZ_STEPS_MAX 256
#define R01S_APU_VIZ_TOKEN 5
#define R01S_APU_VIZ_SYNTH_PER_FRAME 64

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
 * Soft bus window $7F40-$7F5F (A0-A4). Bring-up packing = 8 voices x 4 bytes
 * (see hw/firmware/common/r01_apu_window.h). Legacy smoke (channel 1 / voice 0):
 *   [0] $7F40  bit0=enable, bits4-7=volume (0-15)
 *   [1] $7F41  period low
 *   [2] $7F42  period high (bits0-2) + duty (bits4-5)
 *   [3] $7F43  wave: 0=pulse 1=triangle 2=noise 3=dpcm-stub
 *
 * Eight software voices (BGM 1-5 + SFX 6-8) mix to an 8-bit analog sample
 * (R-2R stand-in). PWM pin still tracks voice-0 pulse for bring-up health.
 * Channel default waves apply on reset / voice_set, not when [3]==0.
 */
typedef struct R01sApuViz {
    int active;
    int step;
    int track_steps;
    int ms_accum;
    int ms_per_nmi; /* ~1000/60 / TEMPO_SCALE — one tracker tick */
    int frames_per_step;
    int nmi_i; /* NMIs since start; step = nmi_i / frames_per_step */
    uint32_t last_ms;
    int dpcm_ms_left; /* DPCM one-shot remaining (viz clock ms) */
    int fe4x_dirty;   /* Host Play wrote regs via FD; board should push M→S2 */
    char cell[R01S_APU_VIZ_STEPS_MAX][R01S_APU_BGM_N][R01S_APU_VIZ_TOKEN];
    uint8_t bytecode[4096];
    uint16_t bytecode_len;
    R01ApuTracker tracker;
} R01sApuViz;

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
    R01sApuViz viz; /* Host Play WAVE feed (silent) */
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
 * Configure a voice for tests / future sequencer (does not write $7F4x).
 * wave = R01sApuWave; duty 0-3; vol 0-15; period >= 2 to run.
 */
void r01s_atmega328p_voice_set(R01sAtmega328p *chip, int ch, uint8_t wave, uint8_t enable, uint8_t vol,
                               uint8_t duty, uint16_t period);

/*
 * Ideal waveform amplitude -128..127 for display column x in [0, width).
 * Fixed time base across the lane (not period-normalized) so pitch reads as
 * cycle density. Uses voice period/duty/wave + phase (scrolls as synth ticks).
 */
int r01s_apu_voice_wave_y(const R01sApuVoice *v, int x, int width);

/* Silent BGM for WAVE monitor. path may be NULL / missing → builtin demo.
 * Prefer Studio export: output/data/bgm_trackN.bin for the track requested by
 * custom_logic r01_bgm_play (same rule as emu Host Play). */
void r01s_atmega328p_viz_start(R01sAtmega328p *chip, uint32_t now_ms, const char *bgm_bin_path);
void r01s_atmega328p_viz_stop(R01sAtmega328p *chip);
/* Advance viz timeline from wall clock; runs a burst of synth ticks for scope. */
void r01s_atmega328p_viz_frame(R01sAtmega328p *chip, uint32_t now_ms);
int r01s_atmega328p_viz_active(const R01sAtmega328p *chip);
int r01s_atmega328p_viz_step(const R01sAtmega328p *chip);
/* Clear after board pushes $7F4x window through MCU-M mailbox. */
int r01s_atmega328p_viz_fe4x_dirty(const R01sAtmega328p *chip);
void r01s_atmega328p_viz_clear_fe4x_dirty(R01sAtmega328p *chip);

#endif
