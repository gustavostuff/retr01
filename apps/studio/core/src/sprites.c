#include "retr01_studio/sprites.h"

#include <string.h>

int r01_chr_find_spr_bank_space(const R01World *w) {
    int bank;
    if (!w) {
        return -1;
    }
    for (bank = 0; bank < R01_SPR_BANKS; bank++) {
        if (w->spr_banks[bank].tile_count < R01_TILES_PER_BANK) {
            return bank;
        }
    }
    return -1;
}

int r01_chr_alloc_spr_tile(R01World *w, int bank) {
    R01ChrBank *b;
    if (!w || bank < 0 || bank >= R01_SPR_BANKS) {
        return -1;
    }
    b = &w->spr_banks[bank];
    /* Bank 0 tile 1 is the cart player stub -- never hand it to user art. */
    if (bank == 0 && b->tile_count == R01_SPR_PLAYER_TILE_ID) {
        if (b->tile_count >= R01_TILES_PER_BANK) {
            return -1;
        }
        memset(b->chr + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES, 0, R01_TILE_BYTES);
        b->tile_count = R01_SPR_PLAYER_TILE_ID + 1;
    }
    if (b->tile_count >= R01_TILES_PER_BANK) {
        return -1;
    }
    memset(b->chr + (size_t)b->tile_count * R01_TILE_BYTES, 0, R01_TILE_BYTES);
    b->tile_count++;
    return b->tile_count - 1;
}

int r01_chr_write_spr_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]) {
    R01ChrBank *b;
    if (!w || !tile || bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return -1;
    }
    b = &w->spr_banks[bank];
    if (tile_id >= b->tile_count) {
        b->tile_count = tile_id + 1;
    }
    memcpy(b->chr + (size_t)tile_id * R01_TILE_BYTES, tile, R01_TILE_BYTES);
    return 0;
}

const uint8_t *r01_chr_spr_tile(const R01World *w, int bank, int tile_id) {
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0) {
        return NULL;
    }
    if (tile_id >= w->spr_banks[bank].tile_count) {
        return NULL;
    }
    return w->spr_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
}

int r01_world_sprite_add(R01World *w, int bank, int tile_id, int pal) {
    R01SpriteDef *s;
    if (!w || w->sprite_count >= R01_MAX_SPRITES) {
        return -1;
    }
    if (bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return -1;
    }
    if (pal < 0) {
        pal = 0;
    }
    if (pal > 3) {
        pal = 3;
    }
    s = &w->sprites[w->sprite_count];
    s->bank = bank;
    s->tile_id = tile_id;
    s->pal = pal;
    w->sprite_count++;
    return w->sprite_count - 1;
}

int r01_world_sprite_remove(R01World *w, int catalog_idx) {
    int i;
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    for (i = catalog_idx; i < w->sprite_count - 1; i++) {
        w->sprites[i] = w->sprites[i + 1];
    }
    w->sprite_count--;
    memset(&w->sprites[w->sprite_count], 0, sizeof(w->sprites[0]));
    return 0;
}

int r01_world_sprite_set_pal(R01World *w, int catalog_idx, int pal) {
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    if (pal < 0) {
        pal = 0;
    }
    if (pal > 3) {
        pal = 3;
    }
    w->sprites[catalog_idx].pal = pal;
    return 0;
}

int r01_world_sprite_move_bank(R01World *w, int catalog_idx, int new_bank) {
    R01SpriteDef *s;
    const uint8_t *src;
    uint8_t copy[R01_TILE_BYTES];
    int new_id;
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    if (new_bank < 0 || new_bank >= R01_SPR_BANKS) {
        return -1;
    }
    s = &w->sprites[catalog_idx];
    if (s->bank == new_bank) {
        return 0;
    }
    src = r01_chr_spr_tile(w, s->bank, s->tile_id);
    if (!src) {
        return -1;
    }
    memcpy(copy, src, R01_TILE_BYTES);
    new_id = r01_chr_alloc_spr_tile(w, new_bank);
    if (new_id < 0) {
        return -1;
    }
    if (r01_chr_write_spr_tile(w, new_bank, new_id, copy) != 0) {
        return -1;
    }
    s->bank = new_bank;
    s->tile_id = new_id;
    return 0;
}
