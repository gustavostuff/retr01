#include "retr01/chr_pack.h"

#include <string.h>

#define BG_TILE_MAX 256

void retr01_ci8x8_to_chr(const uint8_t ci[64], uint8_t out_tile[16])
{
    int y;
    for (y = 0; y < 8; y++) {
        uint8_t p0 = 0;
        uint8_t p1 = 0;
        int x;
        for (x = 0; x < 8; x++) {
            uint8_t c = (uint8_t)(ci[y * 8 + x] & 3u);
            int bit = 7 - x;
            p0 = (uint8_t)(p0 | ((c & 1u) << bit));
            p1 = (uint8_t)(p1 | (((c >> 1) & 1u) << bit));
        }
        out_tile[y] = p0;
        out_tile[8 + y] = p1;
    }
}

static int find_chr_tile(const uint8_t *bank, int used, const uint8_t tile[16])
{
    int i;
    for (i = 0; i < used; i++) {
        if (memcmp(bank + (size_t)i * 16, tile, 16) == 0) {
            return i;
        }
    }
    return -1;
}

static void extract_ci_tile(const uint8_t *ci_plane, int width, int tx, int ty, uint8_t out[64])
{
    int y;
    int x;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int px = tx * 8 + x;
            int py = ty * 8 + y;
            out[y * 8 + x] = (uint8_t)(ci_plane[py * width + px] & 3u);
        }
    }
}

int retr01_count_unique_tiles(const uint8_t tiles[RETR01_SCREEN_TILE_BYTES], int *out_count)
{
    uint8_t seen[BG_TILE_MAX];
    int count = 0;
    int i;

    if (!tiles || !out_count) {
        return -1;
    }

    memset(seen, 0, sizeof(seen));
    for (i = 0; i < RETR01_SCREEN_TILE_BYTES; i++) {
        uint8_t t = tiles[i];
        if (!seen[t]) {
            seen[t] = 1;
            count++;
        }
    }
    *out_count = count;
    return 0;
}

int retr01_pack_canvas(const uint8_t *ci_plane, int width, int height, uint8_t bg_palette_id,
                       uint8_t *chr_bank, size_t chr_bank_bytes, retr01_screen_t *screen_out,
                       int *out_unique_tiles)
{
    uint8_t unique_pixels[BG_TILE_MAX][64];
    int unique = 0;
    int tx;
    int ty;

    if (!ci_plane || !chr_bank || !screen_out || !out_unique_tiles || width != 256 ||
        height != 240 || bg_palette_id > 3) {
        return -1;
    }
    if (chr_bank_bytes < (size_t)BG_TILE_MAX * 16) {
        return -1;
    }

    retr01_screen_clear(screen_out);

    for (ty = 0; ty < RETR01_NT_H; ty++) {
        for (tx = 0; tx < RETR01_NT_W; tx++) {
            uint8_t ci_block[64];
            uint8_t chr_tile[16];
            int match = -1;
            int i;

            extract_ci_tile(ci_plane, width, tx, ty, ci_block);
            retr01_ci8x8_to_chr(ci_block, chr_tile);

            match = find_chr_tile(chr_bank, unique, chr_tile);
            if (match < 0) {
                for (i = 0; i < unique; i++) {
                    if (memcmp(unique_pixels[i], ci_block, 64) == 0) {
                        match = i;
                        break;
                    }
                }
            }

            if (match < 0) {
                if (unique >= BG_TILE_MAX) {
                    return -2;
                }
                memcpy(unique_pixels[unique], ci_block, 64);
                memcpy(chr_bank + (size_t)unique * 16, chr_tile, 16);
                match = unique;
                unique++;
            }

            screen_out->tiles[ty * RETR01_NT_W + tx] = (uint8_t)match;
            retr01_attr_set(screen_out->attrs, tx, ty, bg_palette_id);
        }
    }

    *out_unique_tiles = unique;
    return 0;
}
