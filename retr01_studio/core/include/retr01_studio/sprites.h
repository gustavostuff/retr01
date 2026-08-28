#ifndef retr01_STUDIO_SPRITES_H
#define retr01_STUDIO_SPRITES_H

#include "retr01_studio/types.h"

/* First SPR bank with free CHR slot, or -1 if all full. Prefers bank 0. */
int r01_chr_find_spr_bank_space(const R01World *w);

/* Allocate a blank tile in spr_banks[bank]. Returns tile_id or -1. */
int r01_chr_alloc_spr_tile(R01World *w, int bank);

/* Write 16-byte pattern into spr_banks[bank][tile_id] (grows tile_count). */
int r01_chr_write_spr_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]);

/* Read pointer to SPR tile bytes, or NULL if out of range. */
const uint8_t *r01_chr_spr_tile(const R01World *w, int bank, int tile_id);

/* Append catalog entry. Returns index or -1. */
int r01_world_sprite_add(R01World *w, int bank, int tile_id, int pal);

/* Remove catalog entry by index (does not free CHR). */
int r01_world_sprite_remove(R01World *w, int catalog_idx);

int r01_world_sprite_set_pal(R01World *w, int catalog_idx, int pal);

/*
 * Move sprite CHR to new_bank (alloc + copy). Updates catalog bank/tile_id.
 * Leaves the old CHR slot orphaned. Returns 0 on success.
 */
int r01_world_sprite_move_bank(R01World *w, int catalog_idx, int new_bank);

#endif
