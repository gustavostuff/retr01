#ifndef retr01_STUDIO_PRG_PHASE1_H
#define retr01_STUDIO_PRG_PHASE1_H

#include "retr01_studio/types.h"

/* Phase 1 PRG SoT = Studio Play (play.c). Emits 65C02 that:
 * - boots world 0
 * - reads pads, moves 8x8 player, smooth camera, present-screen collision
 * - X warp → (0,0), Y warp → (1,0)
 * - writes scroll + OAM[0]
 *
 * Zero-page contract (emu soft-syncs camera from these):
 *   $02/$03 player_x  $04/$05 player_y
 *   $06/$07 cam_x     $08/$09 cam_y
 * Play table at $8100: present[8] bitmask + spawn col/row.
 */
void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01World *w);

#endif
