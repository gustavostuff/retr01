#ifndef R01_APU_FD_H
#define R01_APU_FD_H

#include <stdint.h>

#include "r01_spi_mailbox.h"

/*
 * Thin cart hex FD bridge toward the 8x4 $7F40 window (docs/sound.md §5).
 * Encode/apply only — not the full 6502 NMI dual-stream tracker.
 *
 * Frame: FD, mask (bit0=ch1..bit7=ch8), then one payload byte per set bit.
 * Payload:
 *   8X = volume X (0-15)
 *   9X = duty/noise-type X (0-3 used)
 *   7X = DPCM sample id X (channel 5 / index 4 only)
 *   else = note letter byte (docs/sound.md §4) -> period
 */

#define R01_APU_FD_OP 0xFDu
#define R01_APU_FD_MAX_PAYLOAD 8u
#define R01_APU_FD_FRAME_MAX (2u + R01_APU_FD_MAX_PAYLOAD)

void r01_apu_fd_pack_voice(uint8_t *regs, uint8_t ch, uint8_t enable, uint8_t vol, uint8_t duty,
                           uint8_t wave, uint16_t period);

/* Approximate period from note byte (Host Play / bring-up). */
uint16_t r01_apu_fd_note_period(uint8_t note);

/* Encode FD frame. n_payload must equal popcount(mask). Returns length or -1. */
int r01_apu_fd_encode(uint8_t mask, const uint8_t *payload, unsigned n_payload, uint8_t *out,
                      unsigned out_cap);

/* Apply one or more FD frames into regs[R01_APU_REGS]. Returns bytes consumed or -1. */
int r01_apu_fd_apply(uint8_t *regs, const uint8_t *stream, unsigned len);

#endif
