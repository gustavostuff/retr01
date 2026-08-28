#ifndef retr01_STUDIO_PRG_PHASE1_H
#define retr01_STUDIO_PRG_PHASE1_H

#include "retr01_studio/types.h"

/* Flash layout fields needed to patch MAP/palette boot seeks in PRG. */
typedef struct R01PrgCartLayout {
    uint32_t off_pal_bg;
    uint32_t len_pal_bg;
    uint32_t off_pal_spr;
    uint32_t len_pal_spr;
    uint32_t off_map_screen0;
    uint8_t default_pal_row;
} R01PrgCartLayout;

/* Byte offsets within PRG init (CPU $8000+) — must match prg_phase1.c init[]. */
#define R01_PRG_INIT_SCROLL_X 11u /* LDA #imm before STA $FE02 */
#define R01_PRG_INIT_SCROLL_Y 16u /* LDA #imm before STA $FE03 */

/* play_pos_ok @ CPU $8500 (PRG+$0500). Solid shadow dir @ $810A. */
#define R01_PLAY_COLLISION_CPU 0x8500u

/*
 * Phase 1 PRG: reset init, palette + start-screen MAP stream ($FE93->$FE12),
 * then VBlank pad poll. Play table at $8100. Main loop PC stored at PRG+$7FFA.
 * Init scroll is patched from spawn-screen camera (same margin math as Play).
 */
void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01World *w, const R01PrgCartLayout *layout);

#endif
