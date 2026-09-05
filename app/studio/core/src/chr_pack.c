#include "retr01_studio/chr_pack.h"

#include <stdlib.h>
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

uint8_t r01_tile_pixel_color(const uint8_t tile[R01_TILE_BYTES], int sx, int sy) {
    uint8_t p0;
    uint8_t p1;
    int bit;
    if (!tile || sx < 0 || sx > 7 || sy < 0 || sy > 7) {
        return 0;
    }
    p0 = tile[sy];
    p1 = tile[sy + 8];
    bit = 7 - sx;
    return (uint8_t)(((p0 >> bit) & 1u) | (((p1 >> bit) & 1u) << 1));
}

static int tile_equal(const uint8_t a[R01_TILE_BYTES], const uint8_t b[R01_TILE_BYTES]) {
    return memcmp(a, b, R01_TILE_BYTES) == 0;
}

static void tile_flip_h(uint8_t dst[R01_TILE_BYTES], const uint8_t src[R01_TILE_BYTES]) {
    int r;
    for (r = 0; r < 8; r++) {
        int plane;
        for (plane = 0; plane < 2; plane++) {
            uint8_t v = src[r + plane * 8];
            uint8_t o = 0;
            int b;
            for (b = 0; b < 8; b++) {
                if (v & (1u << b)) {
                    o |= (uint8_t)(1u << (7 - b));
                }
            }
            dst[r + plane * 8] = o;
        }
    }
}

static void tile_flip_v(uint8_t dst[R01_TILE_BYTES], const uint8_t src[R01_TILE_BYTES]) {
    int r;
    for (r = 0; r < 8; r++) {
        dst[r] = src[7 - r];
        dst[r + 8] = src[15 - r];
    }
}

static void tile_orient(const uint8_t src[R01_TILE_BYTES], int flip_h, int flip_v, uint8_t dst[R01_TILE_BYTES]) {
    uint8_t tmp[R01_TILE_BYTES];
    if (!flip_h && !flip_v) {
        memcpy(dst, src, R01_TILE_BYTES);
        return;
    }
    if (flip_h && !flip_v) {
        tile_flip_h(dst, src);
        return;
    }
    if (!flip_h && flip_v) {
        tile_flip_v(dst, src);
        return;
    }
    tile_flip_h(tmp, src);
    tile_flip_v(dst, tmp);
}

void r01_tile_orient(const uint8_t src[R01_TILE_BYTES], int flip_h, int flip_v, uint8_t dst[R01_TILE_BYTES]) {
    tile_orient(src, flip_h, flip_v, dst);
}

static int find_unique(uint8_t unique[][R01_TILE_BYTES], int unique_count, const uint8_t tile[R01_TILE_BYTES],
                       int *out_flips) {
    static const int FLIP_VARIANTS[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    int u;
    for (u = 0; u < unique_count; u++) {
        int variant;
        for (variant = 0; variant < 4; variant++) {
            uint8_t oriented[R01_TILE_BYTES];
            int fh = FLIP_VARIANTS[variant][0];
            int fv = FLIP_VARIANTS[variant][1];
            tile_orient(unique[u], fh, fv, oriented);
            if (tile_equal(oriented, tile)) {
                *out_flips = (fh ? R01_ATTR_FLIP_H : 0) | (fv ? R01_ATTR_FLIP_V : 0);
                return u;
            }
        }
    }
    return -1;
}

void r01_screen_fill_pixels_from_bank(const R01World *w, R01Screen *s) {
    int ty, tx;
    if (!w || !s) {
        return;
    }
    for (ty = 0; ty < R01_SCREEN_TILES_Y; ty++) {
        for (tx = 0; tx < R01_SCREEN_TILES_X; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t attr = s->attrs[cell];
            uint8_t tile_id = s->tiles[cell];
            int bank = r01_attr_bank(attr);
            const uint8_t *tile;
            int sy, sx;
            if (bank < 0 || bank >= R01_BG_BANKS || tile_id >= (uint8_t)w->bg_banks[bank].tile_count) {
                continue;
            }
            tile = w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
            for (sy = 0; sy < 8; sy++) {
                for (sx = 0; sx < 8; sx++) {
                    int px = tx * 8 + sx;
                    int py = ty * 8 + sy;
                    int csx = sx;
                    int csy = sy;
                    if (r01_attr_flip_h(attr)) {
                        csx = 7 - csx;
                    }
                    if (r01_attr_flip_v(attr)) {
                        csy = 7 - csy;
                    }
                    s->pixels[py * R01_SCREEN_PX_W + px] = r01_tile_pixel_color(tile, csx, csy);
                }
            }
        }
    }
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
            {
                uint8_t old = s->attrs[cell];
                s->attrs[cell] = r01_attr_merge(old, r01_attr_bank(old), r01_attr_pal(old),
                                                (flips & R01_ATTR_FLIP_H) != 0, (flips & R01_ATTR_FLIP_V) != 0);
            }
        }
    }
    return R01_CHR_OK;
}

void r01_tile_set_pixel(uint8_t tile[R01_TILE_BYTES], int sx, int sy, uint8_t color) {
    int bit;
    if (!tile || sx < 0 || sx > 7 || sy < 0 || sy > 7) {
        return;
    }
    bit = 7 - sx;
    tile[sy] = (uint8_t)((tile[sy] & (uint8_t)~(1u << bit)) | (((color & 1u) ? 1u : 0u) << bit));
    tile[sy + 8] =
        (uint8_t)((tile[sy + 8] & (uint8_t)~(1u << bit)) | (((color & 2u) ? 1u : 0u) << bit));
}

void r01_tile_flood_fill(uint8_t tile[R01_TILE_BYTES], int sx, int sy, uint8_t color) {
    uint8_t seed;
    uint8_t fill = (uint8_t)(color & 3u);
    uint8_t visited[64];
    int queue[64];
    int qhead = 0;
    int qtail = 0;
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    if (!tile || sx < 0 || sx > 7 || sy < 0 || sy > 7) {
        return;
    }
    seed = r01_tile_pixel_color(tile, sx, sy) & 3u;
    if (seed == fill) {
        return;
    }
    memset(visited, 0, sizeof(visited));
    queue[qtail++] = sy * 8 + sx;
    visited[sy * 8 + sx] = 1;
    while (qhead < qtail) {
        int cell = queue[qhead++];
        int cx = cell % 8;
        int cy = cell / 8;
        int d;
        r01_tile_set_pixel(tile, cx, cy, fill);
        for (d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            int ncell;
            if (nx < 0 || ny < 0 || nx > 7 || ny > 7) {
                continue;
            }
            ncell = ny * 8 + nx;
            if (visited[ncell]) {
                continue;
            }
            if ((r01_tile_pixel_color(tile, nx, ny) & 3u) != seed) {
                continue;
            }
            visited[ncell] = 1;
            queue[qtail++] = ncell;
        }
    }
}

static int rgb_brightness(uint8_t r, uint8_t g, uint8_t b) {
    return (int)r + (int)g + (int)b;
}

void r01_tile_from_rgba_brightness(uint8_t out16[R01_TILE_BYTES], const uint8_t *rgba, int img_w, int img_h,
                                   int src_x, int src_y, const uint8_t (*target_rgb)[3]) {
    int sy, sx;
    int tbright[4];
    if (!out16) {
        return;
    }
    memset(out16, 0, R01_TILE_BYTES);
    if (!rgba || !target_rgb || img_w < 1 || img_h < 1) {
        return;
    }
    for (sx = 0; sx < 4; sx++) {
        tbright[sx] = rgb_brightness(target_rgb[sx][0], target_rgb[sx][1], target_rgb[sx][2]);
    }
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            int ix = src_x + sx;
            int iy = src_y + sy;
            const uint8_t *p;
            uint8_t col;
            int bright, best, best_d, i;
            if (ix < 0 || iy < 0 || ix >= img_w || iy >= img_h) {
                continue;
            }
            p = rgba + ((size_t)iy * (size_t)img_w + (size_t)ix) * 4u;
            if (p[3] < 128u) {
                col = 0;
            } else {
                bright = rgb_brightness(p[0], p[1], p[2]);
                best = 0;
                best_d = abs(bright - tbright[0]);
                for (i = 1; i < 4; i++) {
                    int d = abs(bright - tbright[i]);
                    if (d < best_d) {
                        best_d = d;
                        best = i;
                    }
                }
                col = (uint8_t)best;
            }
            r01_tile_set_pixel(out16, sx, sy, col);
        }
    }
}

int r01_chr_alloc_tile(R01World *w, int bank) {
    R01ChrBank *b;
    if (!w || bank < 0 || bank >= R01_BG_BANKS) {
        return -1;
    }
    b = &w->bg_banks[bank];
    if (b->tile_count >= R01_TILES_PER_BANK) {
        return -1;
    }
    memset(b->chr + (size_t)b->tile_count * R01_TILE_BYTES, 0, R01_TILE_BYTES);
    b->tile_count++;
    return b->tile_count - 1;
}

int r01_chr_write_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]) {
    R01ChrBank *b;
    if (!w || !tile || bank < 0 || bank >= R01_BG_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return -1;
    }
    b = &w->bg_banks[bank];
    if (tile_id >= b->tile_count) {
        b->tile_count = tile_id + 1;
    }
    memcpy(b->chr + (size_t)tile_id * R01_TILE_BYTES, tile, R01_TILE_BYTES);
    return 0;
}

void r01_screen_paint_tile(R01World *w, R01Screen *s, int tile_x, int tile_y, uint8_t tile_id, uint8_t attr) {
    int cell;
    int sy, sx;
    const uint8_t *tile;
    int bank;
    if (!w || !s || tile_x < 0 || tile_y < 0 || tile_x >= R01_SCREEN_TILES_X || tile_y >= R01_SCREEN_TILES_Y) {
        return;
    }
    cell = tile_y * R01_SCREEN_TILES_X + tile_x;
    s->tiles[cell] = tile_id;
    s->attrs[cell] = attr;
    bank = r01_attr_bank(attr);
    if (bank < 0 || bank >= R01_BG_BANKS || tile_id >= (uint8_t)w->bg_banks[bank].tile_count) {
        return;
    }
    tile = w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            int px = tile_x * 8 + sx;
            int py = tile_y * 8 + sy;
            int csx = sx;
            int csy = sy;
            if (r01_attr_flip_h(attr)) {
                csx = 7 - csx;
            }
            if (r01_attr_flip_v(attr)) {
                csy = 7 - csy;
            }
            s->pixels[py * R01_SCREEN_PX_W + px] = r01_tile_pixel_color(tile, csx, csy);
        }
    }
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
