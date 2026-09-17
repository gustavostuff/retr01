#ifndef retr01_STUDIO_SPRITES_H
#define retr01_STUDIO_SPRITES_H

#include "retr01_studio/types.h"

/* First SPR bank with free CHR slot, or -1 if all full. Prefers bank 0. */
int r01_chr_find_spr_bank_space(const R01World *w);

/* Prefer first free / next slot. Bank sheets stay contiguous 0..n-1 (no holes). */
int r01_chr_alloc_spr_tile(R01World *w, int bank);

/* Write 16-byte pattern into spr_banks[bank][tile_id] (grows tile_count). */
int r01_chr_write_spr_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]);

/* Read pointer to SPR tile bytes, or NULL if out of range. */
const uint8_t *r01_chr_spr_tile(const R01World *w, int bank, int tile_id);

/* Resolve world SPR or project player bank (bank == R01_PLAYER_CHR_BANK). */
const uint8_t *r01_chr_resolve_spr(const R01Project *p, const R01World *w, int bank, int tile_id);
int r01_chr_write_resolved_spr(R01Project *p, R01World *w, int bank, int tile_id,
                               const uint8_t tile[R01_TILE_BYTES]);

/* Pack non-blank SPR tiles to 0..n-1 and remap entity / metasprite / catalog refs. */
void r01_chr_densify_spr_bank(R01World *w, int bank);
/* Pack non-blank player-bank tiles to 0..n-1 and remap refs across all worlds. */
void r01_project_densify_player_bank(R01Project *p);
/* Pack BG bank tiles after index 0 (tile 0 stays). Remap screens + metatiles. */
void r01_chr_densify_bg_bank(R01World *w, int bank);
/* Densify every BG/SPR bank in every world plus the global player bank. */
void r01_project_densify_all_banks(R01Project *p);

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
