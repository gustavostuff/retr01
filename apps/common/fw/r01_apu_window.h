#ifndef R01_APU_WINDOW_H
#define R01_APU_WINDOW_H

#include <stdint.h>
#include "r01_spi_mailbox.h"

/*
 * Bring-up packing of soft $7F40-$7F5F (32 B) as 8 voices x 4 bytes.
 * SoT channel roles: general_docs/sound.md. Full hex bytecode protocol still later.
 *
 * Per channel at offset ch*4:
 *   [0] bit0=enable, bits4-7=volume (0-15). Triangle ignores vol (full when on).
 *   [1] period low
 *   [2] bits0-2=period high (11-bit period), bits4-5=pulse duty (0-3)
 *   [3] wave: 0=pulse 1=triangle 2=noise 3=dpcm-stub
 *
 * Legacy smoke (ch0 / voice0): $7F40-$7F42 match [0]/[1]/[2]; [3] may be 0 (pulse).
 */

#define R01_APU_CH_N 8u
#define R01_APU_CH_STRIDE 4u

#define R01_APU_WAVE_PULSE 0u
#define R01_APU_WAVE_TRIANGLE 1u
#define R01_APU_WAVE_NOISE 2u
#define R01_APU_WAVE_DPCM 3u

#define R01_APU_CTRL_ENABLE 0x01u
#define R01_APU_CTRL_VOL_SHIFT 4u

static inline uint8_t r01_apu_ch_base(uint8_t ch) {
    return (uint8_t)(ch * R01_APU_CH_STRIDE);
}

static inline int r01_apu_ch_enabled(const uint8_t *regs, uint8_t ch) {
    return (regs[r01_apu_ch_base(ch)] & R01_APU_CTRL_ENABLE) != 0;
}

static inline uint8_t r01_apu_ch_vol(const uint8_t *regs, uint8_t ch) {
    return (uint8_t)((regs[r01_apu_ch_base(ch)] >> R01_APU_CTRL_VOL_SHIFT) & 0x0Fu);
}

static inline uint16_t r01_apu_ch_period(const uint8_t *regs, uint8_t ch) {
    uint8_t b = r01_apu_ch_base(ch);
    return (uint16_t)((regs[b + 1u] | ((regs[b + 2u] & 0x07u) << 8)) & 0x7FFu);
}

static inline uint8_t r01_apu_ch_duty(const uint8_t *regs, uint8_t ch) {
    return (uint8_t)((regs[r01_apu_ch_base(ch) + 2u] >> 4) & 0x03u);
}

static inline uint8_t r01_apu_ch_wave(const uint8_t *regs, uint8_t ch) {
    return (uint8_t)(regs[r01_apu_ch_base(ch) + 3u] & 0x03u);
}

#endif
