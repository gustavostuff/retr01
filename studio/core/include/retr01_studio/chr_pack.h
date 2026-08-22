#ifndef RETR01_STUDIO_CHR_PACK_H
#define RETR01_STUDIO_CHR_PACK_H

#include "retr01_studio/types.h"

typedef enum R01ChrPackStatus {
    R01_CHR_OK = 0,
    R01_CHR_TOO_MANY_TILES = 1,
    R01_CHR_BAD_ARGS = 2
} R01ChrPackStatus;

/* Extract one 8x8 tile (2bpp NES-ish planar) from screen pixels at tile col/row. */
void r01_tile_from_pixels(const uint8_t *pixels, int tile_col, int tile_row, uint8_t out16[R01_TILE_BYTES]);

/* Decode 16-byte CHR tile to 64 pixel indices 0..3. */
void r01_tile_to_pixels(const uint8_t tile16[R01_TILE_BYTES], uint8_t out64[64]);

/* Flip CHR tile in-place style into out16 (H and/or V). */
void r01_tile_flip(const uint8_t in16[R01_TILE_BYTES], int flip_h, int flip_v, uint8_t out16[R01_TILE_BYTES]);

/*
 * Rebuild bg_banks[bank] from all present screens' pixel data.
 * Dedupes 8x8 tiles; prefers FLIP_H/V over duplicate CHR.
 * ANIM tiles get a 4-aligned strip B..B+3 (frames 1-3 copy frame 0).
 * Preserves PAL/SOLID/ANIM; stamps BANK (+ flips from match).
 */
R01ChrPackStatus r01_chr_pack_world_bank(R01World *w, int bank);

#endif
