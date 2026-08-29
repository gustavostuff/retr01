#ifndef retr01_STUDIO_CHR_PACK_H
#define retr01_STUDIO_CHR_PACK_H

#include "retr01_studio/types.h"

typedef enum {
    R01_CHR_OK = 0,
    R01_CHR_BAD_ARGS,
    R01_CHR_TOO_MANY_TILES,
} R01ChrPackStatus;

void r01_tile_from_pixels(const uint8_t *pixels, int tile_col, int tile_row, uint8_t out16[R01_TILE_BYTES]);

uint8_t r01_tile_pixel_color(const uint8_t tile[R01_TILE_BYTES], int sx, int sy);

/* Apply BG attr flips to tile bytes (self-inverse: same call undoes). */
void r01_tile_orient(const uint8_t src[R01_TILE_BYTES], int flip_h, int flip_v, uint8_t dst[R01_TILE_BYTES]);

/* Rebuild per-pixel color indices from tile map + BG bank (inverse of pack). */
void r01_screen_fill_pixels_from_bank(const R01World *w, R01Screen *s);

void r01_tile_set_pixel(uint8_t tile[R01_TILE_BYTES], int sx, int sy, uint8_t color);
/*
 * Quantize an 8x8 window from RGBA into a NES-style 2bpp tile.
 * Transparent (alpha < 128) -> index 0. Opaque pixels match the nearest of
 * target_rgb[0..3] by brightness (r+g+b). src may be smaller than 8x8 (rest -> 0).
 * target_rgb is 4 entries of {r,g,b}.
 */
void r01_tile_from_rgba_brightness(uint8_t out16[R01_TILE_BYTES], const uint8_t *rgba, int img_w, int img_h,
                                   int src_x, int src_y, const uint8_t (*target_rgb)[3]);

/* Ensure BG bank 0 has at least one blank tile; returns tile index or -1. */
int r01_chr_alloc_tile(R01World *w, int bank);
/* Write 16-byte pattern into bank[tile_id] (grows tile_count if needed). */
int r01_chr_write_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]);
/* Stamp tile+attr into screen map and refresh that cell's pixels. */
void r01_screen_paint_tile(R01World *w, R01Screen *s, int tile_x, int tile_y, uint8_t tile_id, uint8_t attr);

/* Pack all present screens into BG bank 0 (max 256 unique tiles). */
R01ChrPackStatus r01_chr_pack_world_bank0(R01World *w);

#endif
