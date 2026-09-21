#ifndef R01_APU_TRACKER_H
#define R01_APU_TRACKER_H

#include <stdint.h>

#include "r01_apu_fd.h"
#include "r01_spi_mailbox.h"

/*
 * 6502-side NMI dual-stream tracker (docs/general/sound.md), host C MVP.
 * BGM (ch1-5) + SFX (ch6-8). Expands FD into the 8x4 $7F40 window.
 * S2 never parses this stream. Shared by emu / Host Play.
 */

#define R01_APU_CTRL_FE 0xFEu
#define R01_APU_CTRL_FA 0xFAu
#define R01_APU_CTRL_FB 0xFBu

#define R01_APU_BGM_MASK 0x1Fu
#define R01_APU_SFX_MASK 0xE0u

#define R01_APU_SFX_X 1u
#define R01_APU_SFX_Y 2u

typedef struct R01ApuStreamSm {
    const uint8_t *rom;
    uint16_t len;
    uint16_t pc;
    uint8_t delay;
    uint8_t active;
    uint8_t loop; /* 1 after FA seen as default; FA sets loop+rewind */
} R01ApuStreamSm;

typedef struct R01ApuTracker {
    R01ApuStreamSm bgm;
    R01ApuStreamSm sfx;
} R01ApuTracker;

void r01_apu_tracker_init(R01ApuTracker *t);
void r01_apu_tracker_set_bgm(R01ApuTracker *t, const uint8_t *rom, uint16_t len);
void r01_apu_tracker_trigger_sfx(R01ApuTracker *t, const uint8_t *rom, uint16_t len);

/* Short SFX bytecode for voices 6-8. Returns length or -1. */
int r01_apu_sfx_encode(uint8_t id, uint8_t *out, unsigned out_cap);

/* One NMI / frame. Applies FD into regs. Returns 1 if any stream advanced. */
int r01_apu_tracker_nmi(R01ApuTracker *t, uint8_t *regs);

#endif
