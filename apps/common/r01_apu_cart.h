#ifndef R01_APU_CART_H
#define R01_APU_CART_H

#include <stdint.h>

/*
 * Phase 1 BGM in PRG (docs/general/memory.md, docs/general/sound.md).
 * CPU $8000 = PRG+0. Solid collision tables can fill through ~$A500, so the
 * stream blob starts at $B000. Vectors stay at $FFFA.
 *
 * $80FE: boot track, 1-based. 0 = no autoplay.
 * $B000 blob:
 *   +0  'B' 'G'
 *   +2  u8 track_count (0..8)
 *   +3  u8 reserved
 *   +4  u16 off[8] from blob base (0 = empty slot)
 *   +20 u16 len[8]
 *   +36 payloads (FD/FE/FA bytecode)
 */

#define R01_PRG_BGM_BOOT_OFF 0x00FEu
#define R01_PRG_BGM_OFF 0x3000u
#define R01_PRG_BGM_END 0x7FFAu
#define R01_PRG_BGM_TRACKS 8u
#define R01_PRG_BGM_MAGIC0 ((uint8_t)'B')
#define R01_PRG_BGM_MAGIC1 ((uint8_t)'G')
#define R01_PRG_BGM_HDR 36u

#endif
