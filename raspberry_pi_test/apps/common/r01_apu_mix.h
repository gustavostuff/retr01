#ifndef R01_APU_MIX_H
#define R01_APU_MIX_H

#include <stdint.h>

#include "r01_apu_window.h"
#include "r01_spi_mailbox.h"

/*
 * Host mix of the 8x4 $7F40 window (docs/general/sound.md).
 * Same voices S2 PWM would mix. Not S2 PWM.
 */

#define R01_APU_DPCM_HZ 16000

#define R01_APU_INS_GUITAR 0
#define R01_APU_INS_EGUITAR 1
#define R01_APU_INS_PIANO 2
#define R01_APU_INS_FLUTE 3
#define R01_APU_INS_COUNT 4

typedef struct R01ApuMixVoice {
    double phase;
    double env;
    uint16_t lfsr;
    uint16_t last_per;
    int dpcm_frac;
    int dpcm_bits_left;
    unsigned dpcm_bit_i;
    int8_t dpcm_acc;
    uint8_t last_en;
    uint8_t last_dpcm_id;
} R01ApuMixVoice;

typedef struct R01ApuMix {
    int sample_rate;
    double env_mul[R01_APU_INS_COUNT];
    uint8_t ins[R01_APU_CH_N];
    R01ApuMixVoice v[R01_APU_CH_N];
    uint8_t regs[R01_APU_REGS];
} R01ApuMix;

void r01_apu_mix_init(R01ApuMix *m, int sample_rate);
void r01_apu_mix_set_regs(R01ApuMix *m, const uint8_t *regs);
void r01_apu_mix_set_ins(R01ApuMix *m, const uint8_t ins[R01_APU_CH_N]);
void r01_apu_mix_render(R01ApuMix *m, int16_t *out, int frames);

#endif
