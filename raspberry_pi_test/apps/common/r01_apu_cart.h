#ifndef R01_APU_CART_H
#define R01_APU_CART_H

#include <stdint.h>

/*
 * Compressed BGM lives in the cart image (MAP), not in the 32 KB PRG window.
 * Blob layout: memory.md. Boot track index is a PRG byte at $80FE.
 *
 * Blob:
 *   +0  'B' 'G'
 *   +2  u8 track_count (0..8)
 *   +3  u8 1 = ins table at +36 (0 = payloads start at +36)
 *   +4  u16 off[8] from blob base (0 = empty slot)
 *   +20 u16 len[8]
 *   +36 ins[8][5] when byte 3 is 1 (BGM 1-5: Guitar/EGuitar/Piano/Flute)
 *   +76 payloads (FD/FE/FA bytecode)
 */

#define R01_PRG_BGM_BOOT_OFF 0x00FEu
#define R01_BGM_TRACKS 8u
#define R01_BGM_MAGIC0 ((uint8_t)'B')
#define R01_BGM_MAGIC1 ((uint8_t)'G')
#define R01_BGM_HDR 36u
#define R01_BGM_INS_VER 1u
#define R01_BGM_INS_CH 5u
#define R01_BGM_INS_MAX 3u /* 0 guitar .. 3 flute */
#define R01_BGM_INS_BYTES (R01_BGM_TRACKS * R01_BGM_INS_CH)
#define R01_BGM_HDR_V1 (R01_BGM_HDR + R01_BGM_INS_BYTES)
#define R01_CART_BGM_BLOB_MAX (48u * 1024u)

/* Names used by pack / emu (same values). */
#define R01_PRG_BGM_TRACKS R01_BGM_TRACKS
#define R01_PRG_BGM_MAGIC0 R01_BGM_MAGIC0
#define R01_PRG_BGM_MAGIC1 R01_BGM_MAGIC1
#define R01_PRG_BGM_HDR R01_BGM_HDR
#define R01_PRG_BGM_INS_VER R01_BGM_INS_VER
#define R01_PRG_BGM_INS_CH R01_BGM_INS_CH
#define R01_PRG_BGM_INS_MAX R01_BGM_INS_MAX
#define R01_PRG_BGM_INS_BYTES R01_BGM_INS_BYTES
#define R01_PRG_BGM_HDR_V1 R01_BGM_HDR_V1

#endif
