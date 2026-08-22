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

void r01_tile_to_pixels(const uint8_t tile16[R01_TILE_BYTES], uint8_t out64[64]) {
    int row, col;
    for (row = 0; row < 8; row++) {
        uint8_t p0 = tile16[row];
        uint8_t p1 = tile16[row + 8];
        for (col = 0; col < 8; col++) {
            int bit = 7 - col;
            uint8_t c = 0;
            if (p0 & (1u << bit)) {
                c |= 1u;
            }
            if (p1 & (1u << bit)) {
                c |= 2u;
            }
            out64[row * 8 + col] = c;
        }
    }
}

static void flip_plane_h(uint8_t plane[8]) {
    int r;
    for (r = 0; r < 8; r++) {
        uint8_t v = plane[r];
        uint8_t o = 0;
        int b;
        for (b = 0; b < 8; b++) {
            if (v & (1u << b)) {
                o |= (uint8_t)(1u << (7 - b));
            }
        }
        plane[r] = o;
    }
}

static void flip_plane_v(uint8_t plane[8]) {
    int r;
    for (r = 0; r < 4; r++) {
        uint8_t tmp = plane[r];
        plane[r] = plane[7 - r];
        plane[7 - r] = tmp;
    }
}

void r01_tile_flip(const uint8_t in16[R01_TILE_BYTES], int flip_h, int flip_v, uint8_t out16[R01_TILE_BYTES]) {
    memcpy(out16, in16, R01_TILE_BYTES);
    if (flip_h) {
        flip_plane_h(out16);
        flip_plane_h(out16 + 8);
    }
    if (flip_v) {
        flip_plane_v(out16);
        flip_plane_v(out16 + 8);
    }
}

static int tile_equal(const uint8_t a[R01_TILE_BYTES], const uint8_t b[R01_TILE_BYTES]) {
    return memcmp(a, b, R01_TILE_BYTES) == 0;
}

/* Match tile against unique set; prefer identical, else H/V/HV flip of an existing tile. */
static int find_unique(uint8_t unique[][R01_TILE_BYTES], int unique_count, const uint8_t tile[R01_TILE_BYTES],
                       int *out_flips) {
    int u;
    for (u = 0; u < unique_count; u++) {
        uint8_t tmp[R01_TILE_BYTES];
        if (tile_equal(unique[u], tile)) {
            *out_flips = 0;
            return u;
        }
        r01_tile_flip(unique[u], 1, 0, tmp);
        if (tile_equal(tmp, tile)) {
            *out_flips = R01_ATTR_FLIP_H;
            return u;
        }
        r01_tile_flip(unique[u], 0, 1, tmp);
        if (tile_equal(tmp, tile)) {
            *out_flips = R01_ATTR_FLIP_V;
            return u;
        }
        r01_tile_flip(unique[u], 1, 1, tmp);
        if (tile_equal(tmp, tile)) {
            *out_flips = R01_ATTR_FLIP_H | R01_ATTR_FLIP_V;
            return u;
        }
    }
    return -1;
}

static int ensure_slots(uint8_t unique[][R01_TILE_BYTES], int *unique_count, int need) {
    if (need > R01_TILES_PER_BANK) {
        return -1;
    }
    while (*unique_count < need) {
        memset(unique[*unique_count], 0, R01_TILE_BYTES);
        (*unique_count)++;
    }
    return 0;
}

static R01ChrPackStatus pack_tilemap(uint8_t unique[][R01_TILE_BYTES], int *unique_count, uint8_t *pixels,
                                     uint8_t *tiles, uint8_t *attrs, int bank) {
    int ty, tx;
    for (ty = 0; ty < R01_SCREEN_TILES_Y; ty++) {
        for (tx = 0; tx < R01_SCREEN_TILES_X; tx++) {
            uint8_t tile[R01_TILE_BYTES];
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t prev = attrs[cell];
            uint8_t keep = (uint8_t)(prev & (R01_ATTR_PAL_MASK | R01_ATTR_SOLID | R01_ATTR_ANIM));
            int flips = 0;
            int found;

            r01_tile_from_pixels(pixels, tx, ty, tile);

            if (r01_attr_anim(prev)) {
                int base = (*unique_count + 3) & ~3;
                if (ensure_slots(unique, unique_count, base + 4) != 0) {
                    return R01_CHR_TOO_MANY_TILES;
                }
                memcpy(unique[base], tile, R01_TILE_BYTES);
                memcpy(unique[base + 1], tile, R01_TILE_BYTES);
                memcpy(unique[base + 2], tile, R01_TILE_BYTES);
                memcpy(unique[base + 3], tile, R01_TILE_BYTES);
                tiles[cell] = (uint8_t)base;
                attrs[cell] = (uint8_t)(keep | (uint8_t)(bank & R01_ATTR_BANK_MASK) | R01_ATTR_ANIM);
                continue;
            }

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
            tiles[cell] = (uint8_t)found;
            attrs[cell] = (uint8_t)(keep | (uint8_t)(bank & R01_ATTR_BANK_MASK) | (uint8_t)flips);
        }
    }
    return R01_CHR_OK;
}

R01ChrPackStatus r01_chr_pack_world_bank(R01World *w, int bank) {
    uint8_t unique[R01_TILES_PER_BANK][R01_TILE_BYTES];
    int unique_count = 0;
    int si, pi;
    R01ChrPackStatus st;

    if (!w || bank < 0 || bank >= R01_BG_BANKS) {
        return R01_CHR_BAD_ARGS;
    }

    memset(w->bg_banks[bank].chr, 0, R01_BANK_CHR_BYTES);
    w->bg_banks[bank].tile_count = 0;

    for (si = 0; si < w->screen_count; si++) {
        R01Screen *s = &w->screens[si];
        if (!s->present) {
            continue;
        }
        st = pack_tilemap(unique, &unique_count, s->pixels, s->tiles, s->attrs, bank);
        if (st != R01_CHR_OK) {
            return st;
        }
    }
    for (pi = 0; pi < R01_MAX_PARALLAX_PLANES; pi++) {
        R01ParallaxPlane *pl = &w->planes[pi];
        if (!pl->present) {
            continue;
        }
        st = pack_tilemap(unique, &unique_count, pl->pixels, pl->tiles, pl->attrs, bank);
        if (st != R01_CHR_OK) {
            return st;
        }
    }

    memcpy(w->bg_banks[bank].chr, unique, (size_t)unique_count * R01_TILE_BYTES);
    w->bg_banks[bank].tile_count = unique_count;
    return R01_CHR_OK;
}
