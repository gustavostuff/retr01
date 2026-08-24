#ifndef RETR01_STUDIO_CHR_PACK_H
#define RETR01_STUDIO_CHR_PACK_H

#include "retr01_studio/types.h"

typedef enum {
    R01_CHR_OK = 0,
    R01_CHR_BAD_ARGS,
    R01_CHR_TOO_MANY_TILES,
} R01ChrPackStatus;

void r01_tile_from_pixels(const uint8_t *pixels, int tile_col, int tile_row, uint8_t out16[R01_TILE_BYTES]);

uint8_t r01_tile_pixel_color(const uint8_t tile[R01_TILE_BYTES], int sx, int sy);

/* Pack all present screens into BG bank 0 (max 256 unique tiles). */
R01ChrPackStatus r01_chr_pack_world_bank0(R01World *w);

#endif
