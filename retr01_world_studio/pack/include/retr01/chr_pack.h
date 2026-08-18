#ifndef RETR01_CHR_PACK_H
#define RETR01_CHR_PACK_H

#include <stdint.h>

#include "retr01/screen.h"
#include "retr01/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convert 8x8 color indices (0..3) to 16-byte 2bpp planar tile. */
void retr01_ci8x8_to_chr(const uint8_t ci[64], uint8_t out_tile[16]);

/* Pack 256x240 canvas (ci 0..3 per pixel) into nametable + deduped CHR bank. */
int retr01_pack_canvas(const uint8_t *ci_plane, int width, int height, uint8_t bg_palette_id,
                       uint8_t *chr_bank, size_t chr_bank_bytes, retr01_screen_t *screen_out,
                       int *out_unique_tiles);

/* Count unique tile indices used in nametable (must be <= 256). */
int retr01_count_unique_tiles(const uint8_t tiles[RETR01_SCREEN_TILE_BYTES], int *out_count);

#ifdef __cplusplus
}
#endif

#endif
