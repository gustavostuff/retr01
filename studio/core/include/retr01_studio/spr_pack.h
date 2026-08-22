#ifndef RETR01_STUDIO_SPR_PACK_H
#define RETR01_STUDIO_SPR_PACK_H

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/types.h"

/*
 * Ensure spr_banks[bank] tile_count covers tile_index (extends with blank tiles).
 * Returns 0 ok, -1 bad args / overflow.
 */
int r01_spr_ensure_tile(R01World *w, int bank, int tile_index);

/*
 * Plot color 0..3 into an 8x8 CHR tile in spr_banks[bank] at pixel (px,py) within tile.
 */
void r01_spr_tile_plot(R01World *w, int bank, int tile_index, int px, int py, uint8_t color);

uint8_t r01_spr_tile_get_pixel(const R01World *w, int bank, int tile_index, int px, int py);

/*
 * Compact + dedupe spr_banks[bank]: drop unused trailing blanks among used OAM/meta
 * tiles, merge identical CHR, remap OAM/meta indices that use this bank.
 * Prefer flips when matching (same as BG pack).
 */
R01ChrPackStatus r01_spr_pack_world_bank(R01World *w, int bank);

#endif
