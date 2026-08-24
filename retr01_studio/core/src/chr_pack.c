#include "retr01_studio/chr_pack.h"

#include <string.h>

void r01_tile_from_pixels(const uint8_t *pixels, int tile_col, int tile_row, uint8_t out16[R01_TILE_BYTES]) {
    int row;
    memset(out16, 0, R01_TILE_BYTES);
    for (row = 0; row < 8; row++) {
        uint8_t p0 = 0, p1 = 0;
        int col;
        int py = tile_row * 8 + row;
        for (col = 0; col < 8; col++) {
            int px = tile_col * 8 + col;
            uint8_t c = pixels[py * R01_SCREEN_PX_W + px] & 3u;
            int bit = 7 - col;
            if (c & 1u) {
                p0 |= (uint8_t)(1u << bit);
            }
            if (c & 2u) {
                p1 |= (uint8_t)(1u << bit);
            }
        }
        out16[row] = p0;
        out16[row + 8] = p1;
    }
}

static int tile_equal(const uint8_t a[R01_TILE_BYTES], const uint8_t b[R01_TILE_BYTES]) {
    return memcmp(a, b, R01_TILE_BYTES) == 0;
}

static int find_unique(uint8_t unique[][R01_TILE_BYTES], int unique_count, const uint8_t tile[R01_TILE_BYTES],
                       int *out_flips) {
    int u;
    for (u = 0; u < unique_count; u++) {
        uint8_t tmp[R01_TILE_BYTES];
        if (tile_equal(unique[u], tile)) {
            *out_flips = 0;
            return u;
        }
        /* H flip */
        memcpy(tmp, unique[u], R01_TILE_BYTES);
        {
            int r;
            for (r = 0; r < 8; r++) {
                uint8_t v = tmp[r];
                uint8_t o = 0;
                int b;
                for (b = 0; b < 8; b++) {
                    if (v & (1u << b)) {
                        o |= (uint8_t)(1u << (7 - b));
                    }
                }
                tmp[r] = o;
                tmp[r + 8] = unique[u][r + 8];
            }
        }
        if (tile_equal(tmp, tile)) {
            *out_flips = R01_ATTR_FLIP_H;
            return u;
        }
    }
    return -1;
}

static R01ChrPackStatus pack_screen(uint8_t unique[][R01_TILE_BYTES], int *unique_count, R01Screen *s) {
    int ty, tx;
    for (ty = 0; ty < R01_SCREEN_TILES_Y; ty++) {
        for (tx = 0; tx < R01_SCREEN_TILES_X; tx++) {
            uint8_t tile[R01_TILE_BYTES];
            int cell = ty * R01_SCREEN_TILES_X + tx;
            int flips = 0;
            int found;
            r01_tile_from_pixels(s->pixels, tx, ty, tile);
            found = find_unique(unique, *unique_count, tile, &flips);
            if (found < 0) {
                if (*unique_count >= R01_TILES_PER_BANK) {
                    return R01_CHR_TOO_MANY_TILES;
                }
                memcpy(unique[*unique_count], tile, R01_TILE_BYTES);
                found = *unique_count;
                (*unique_count)++;
                flips = 0;
            }
            s->tiles[cell] = (uint8_t)found;
            s->attrs[cell] = r01_attr_pack(0, 0, flips & R01_ATTR_FLIP_H ? 1 : 0, flips & R01_ATTR_FLIP_V ? 1 : 0);
        }
    }
    return R01_CHR_OK;
}

R01ChrPackStatus r01_chr_pack_world_bank0(R01World *w) {
    uint8_t unique[R01_TILES_PER_BANK][R01_TILE_BYTES];
    int unique_count = 0;
    int si;
    if (!w) {
        return R01_CHR_BAD_ARGS;
    }
    memset(w->bg_banks[0].chr, 0, R01_BANK_CHR_BYTES);
    w->bg_banks[0].tile_count = 0;
    for (si = 0; si < w->screen_count; si++) {
        R01Screen *s = &w->screens[si];
        R01ChrPackStatus st;
        if (!s->present) {
            continue;
        }
        st = pack_screen(unique, &unique_count, s);
        if (st != R01_CHR_OK) {
            return st;
        }
    }
    memcpy(w->bg_banks[0].chr, unique, (size_t)unique_count * R01_TILE_BYTES);
    w->bg_banks[0].tile_count = unique_count;
    return R01_CHR_OK;
}
