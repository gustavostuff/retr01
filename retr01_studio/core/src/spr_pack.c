#include "retr01_studio/spr_pack.h"

#include <string.h>

static void tile_plot_chr(uint8_t tile16[R01_TILE_BYTES], int px, int py, uint8_t color) {
    uint8_t pix[64];
    int i;
    r01_tile_to_pixels(tile16, pix);
    if (px < 0 || py < 0 || px >= 8 || py >= 8) {
        return;
    }
    pix[py * 8 + px] = (uint8_t)(color & 3u);
    memset(tile16, 0, R01_TILE_BYTES);
    for (i = 0; i < 8; i++) {
        uint8_t p0 = 0, p1 = 0;
        int col;
        for (col = 0; col < 8; col++) {
            uint8_t c = pix[i * 8 + col] & 3u;
            int bit = 7 - col;
            if (c & 1u) {
                p0 |= (uint8_t)(1u << bit);
            }
            if (c & 2u) {
                p1 |= (uint8_t)(1u << bit);
            }
        }
        tile16[i] = p0;
        tile16[i + 8] = p1;
    }
}

int r01_spr_ensure_tile(R01World *w, int bank, int tile_index) {
    R01SprBank *b;
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile_index < 0 || tile_index >= R01_TILES_PER_BANK) {
        return -1;
    }
    b = &w->spr_banks[bank];
    if (tile_index >= b->tile_count) {
        b->tile_count = tile_index + 1;
    }
    return 0;
}

void r01_spr_tile_plot(R01World *w, int bank, int tile_index, int px, int py, uint8_t color) {
    if (r01_spr_ensure_tile(w, bank, tile_index) != 0) {
        return;
    }
    tile_plot_chr(&w->spr_banks[bank].chr[tile_index * R01_TILE_BYTES], px, py, color);
}

uint8_t r01_spr_tile_get_pixel(const R01World *w, int bank, int tile_index, int px, int py) {
    uint8_t pix[64];
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile_index < 0 || tile_index >= R01_TILES_PER_BANK) {
        return 0;
    }
    if (tile_index >= w->spr_banks[bank].tile_count) {
        return 0;
    }
    r01_tile_to_pixels(&w->spr_banks[bank].chr[tile_index * R01_TILE_BYTES], pix);
    if (px < 0 || py < 0 || px >= 8 || py >= 8) {
        return 0;
    }
    return pix[py * 8 + px] & 3u;
}

R01ChrPackStatus r01_spr_pack_world_bank(R01World *w, int bank) {
    uint8_t unique[R01_TILES_PER_BANK][R01_TILE_BYTES];
    int unique_count = 0;
    int map[R01_TILES_PER_BANK];
    int t, si, mi, pi;
    R01SprBank *src;

    if (!w || bank < 0 || bank >= R01_SPR_BANKS) {
        return R01_CHR_BAD_ARGS;
    }

    src = &w->spr_banks[bank];
    for (t = 0; t < R01_TILES_PER_BANK; t++) {
        map[t] = -1;
    }

    for (t = 0; t < src->tile_count; t++) {
        int u, found = -1;
        const uint8_t *tile = &src->chr[t * R01_TILE_BYTES];
        for (u = 0; u < unique_count; u++) {
            if (memcmp(unique[u], tile, R01_TILE_BYTES) == 0) {
                found = u;
                break;
            }
        }
        if (found < 0) {
            if (unique_count >= R01_TILES_PER_BANK) {
                return R01_CHR_TOO_MANY_TILES;
            }
            memcpy(unique[unique_count], tile, R01_TILE_BYTES);
            found = unique_count;
            unique_count++;
        }
        map[t] = found;
    }

    memcpy(src->chr, unique, (size_t)unique_count * R01_TILE_BYTES);
    if (unique_count < R01_TILES_PER_BANK) {
        memset(src->chr + unique_count * R01_TILE_BYTES, 0,
               (size_t)(R01_TILES_PER_BANK - unique_count) * R01_TILE_BYTES);
    }
    src->tile_count = unique_count;

    for (si = 0; si < w->screen_count; si++) {
        R01Screen *s = &w->screens[si];
        int oi;
        if (!s->present) {
            continue;
        }
        for (oi = 0; oi < s->oam_count; oi++) {
            R01Oam *o = &s->oam[oi];
            if (r01_attr_bank(o->attr) != bank) {
                continue;
            }
            if (map[o->tile] >= 0) {
                o->tile = (uint8_t)map[o->tile];
            }
        }
    }

    for (mi = 0; mi < w->meta_count; mi++) {
        R01MetaSprite *m = &w->metas[mi];
        if (!m->present) {
            continue;
        }
        for (pi = 0; pi < m->part_count; pi++) {
            R01MetaPart *part = &m->parts[pi];
            if (r01_attr_bank(part->attr) != bank) {
                continue;
            }
            if (map[part->tile] >= 0) {
                part->tile = (uint8_t)map[part->tile];
            }
        }
    }

    return R01_CHR_OK;
}
