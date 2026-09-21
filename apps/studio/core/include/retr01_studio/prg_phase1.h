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

/* Byte offsets within PRG init (CPU $8000+) -- must match prg_phase1.c init[]. */
#define R01_PRG_INIT_SCROLL_X 11u /* LDA #imm before STA $7F02 */
#define R01_PRG_INIT_SCROLL_Y 16u /* LDA #imm before STA $7F03 */

/* play_pos_ok @ CPU $8500 (PRG+$0500). Solid pattern list @ $8700, RAM copy $0200. */
#define R01_PLAY_COLLISION_CPU 0x8500u
#define R01_PLAY_SOLID_RAM 0x0200u
#define R01_PLAY_SOLID_LIST_CPU 0x8700u

/* Entity placements in PRG (not cart world blob). CPU $81C0 / PRG+$01C0. */
#define R01_PRG_PLAY_INST_COUNT_OFF 0x01C0u
#define R01_PRG_PLAY_INST_TABLE_OFF 0x01C1u
#define R01_PRG_PLAY_SPAWN_CELL_OFF 0x0120u
#define R01_PRG_R01P_OFF 0x00F0u
#define R01_PRG_PLAT_GRAVITY_OFF 0x00F7u
#define R01_PRG_PLAT_JUMP_OFF 0x00F8u
#define R01_PRG_PLAT_METER_OFF 0x00F9u
#define R01_PRG_PLAT_CROUCH_OFF 0x00FAu
#define R01_PRG_PLAYER_ANIM_IDLE_OFF 0x00FBu
#define R01_PRG_PLAYER_ANIM_WALK_OFF 0x00FCu
#define R01_PRG_PLAYER_ANIM_JUMP_OFF 0x00FDu

/*
 * Phase 1 PRG: reset init, palette + start-screen MAP stream ($7F93->$7F12),
 * then VBlank pad poll. Play table at $8100. Main loop PC stored at PRG+$7FFA.
 * Init scroll is patched from spawn-screen camera (same margin math as Play).
 */
void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout);

#endif
