#include "retr01/chr_pack.h"

#include "retr01/project.h"

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

static void clear_sprite_page(uint8_t *chr_bank, size_t chr_bank_bytes)
{
    if (chr_bank_bytes >= (size_t)RETR01_CHR_BANK_TILES * RETR01_CHR_TILE_BYTES) {
        memset(chr_bank + (size_t)RETR01_CHR_BG_TILES * RETR01_CHR_TILE_BYTES, 0,
               (size_t)RETR01_CHR_SPR_TILES * RETR01_CHR_TILE_BYTES);
    }
}

int retr01_pack_canvases(const uint8_t *const *ci_planes, const uint8_t *bg_palette_ids, int count,
                         uint8_t *chr_bank, size_t chr_bank_bytes, retr01_screen_t *screens_inout,
                         int *out_unique_tiles)
{
    uint8_t unique_pixels[BG_TILE_MAX][64];
    int unique = 0;
    int n;
    int tx;
    int ty;

    if (!ci_planes || !bg_palette_ids || !chr_bank || !screens_inout || !out_unique_tiles ||
        count <= 0) {
        return -1;
    }
    if (chr_bank_bytes < (size_t)BG_TILE_MAX * 16) {
        return -1;
    }

    memset(chr_bank, 0, (size_t)BG_TILE_MAX * 16);
    clear_sprite_page(chr_bank, chr_bank_bytes);

    for (n = 0; n < count; n++) {
        uint8_t col = screens_inout[n].col;
        uint8_t row = screens_inout[n].row;
        uint8_t flags = screens_inout[n].flags;
        uint8_t authored = screens_inout[n].authored_bank;
        uint8_t pal = bg_palette_ids[n] & 3u;

        if (!ci_planes[n]) {
            return -1;
        }

        retr01_screen_clear(&screens_inout[n]);
        screens_inout[n].col = col;
        screens_inout[n].row = row;
        screens_inout[n].flags = flags;
        screens_inout[n].authored_bank = authored;

        for (ty = 0; ty < RETR01_NT_H; ty++) {
            for (tx = 0; tx < RETR01_NT_W; tx++) {
                uint8_t ci_block[64];
                uint8_t chr_tile[16];
                int match;

                extract_ci_tile(ci_planes[n], RETR01_CANVAS_W, tx, ty, ci_block);
                retr01_ci8x8_to_chr(ci_block, chr_tile);
                match = find_chr_tile(chr_bank, unique, chr_tile);

                if (match < 0) {
                    if (unique >= BG_TILE_MAX) {
                        return -2;
                    }
                    memcpy(unique_pixels[unique], ci_block, 64);
                    memcpy(chr_bank + (size_t)unique * 16, chr_tile, 16);
                    match = unique;
                    unique++;
                }

                screens_inout[n].tiles[ty * RETR01_NT_W + tx] = (uint8_t)match;
                retr01_attr_set(screens_inout[n].attrs, tx, ty, pal);
            }
        }
    }

    *out_unique_tiles = unique;
    return 0;
}

int retr01_pack_canvas(const uint8_t *ci_plane, int width, int height, uint8_t bg_palette_id,
                       uint8_t *chr_bank, size_t chr_bank_bytes, retr01_screen_t *screen_out,
                       int *out_unique_tiles)
{
    if (!ci_plane || width != RETR01_CANVAS_W || height != RETR01_CANVAS_H) {
        return -1;
    }
    return retr01_pack_canvases(&ci_plane, &bg_palette_id, 1, chr_bank, chr_bank_bytes, screen_out,
                                out_unique_tiles);
}

int retr01_project_pack(retr01_project_t *proj)
{
    const uint8_t *planes[RETR01_PROJECT_MAX_SCREENS];
    uint8_t pals[RETR01_PROJECT_MAX_SCREENS];
    retr01_screen_t packed[RETR01_PROJECT_MAX_SCREENS];
    int indices[RETR01_PROJECT_MAX_SCREENS];
    int n = 0;
    int i;
    int unique = 0;
    int bank;
    int has_pixels = 0;
    int rc;

    if (!proj || proj->screen_count <= 0) {
        return -1;
    }

    bank = proj->active_bank;
    if (bank < 0 || bank > 3) {
        bank = 0;
    }

    for (i = 0; i < proj->screen_count; i++) {
        retr01_project_screen_t *ps = &proj->screens[i];
        size_t p;
        if (!ps->canvas) {
            return -1;
        }
        for (p = 0; p < RETR01_CANVAS_BYTES; p++) {
            if (ps->canvas[p] != 0) {
                has_pixels = 1;
                break;
            }
        }
        planes[n] = ps->canvas;
        pals[n] = ps->canvas_palette;
        packed[n] = ps->screen;
        indices[n] = i;
        n++;
    }

    if (n <= 0 || !has_pixels) {
        return 0;
    }

    rc = retr01_pack_canvases(planes, pals, n, proj->chr_banks[bank], RETR01_CHR_BANK_BYTES, packed,
                              &unique);
    if (rc != 0) {
        return rc;
    }

    for (i = 0; i < n; i++) {
        retr01_project_screen_t *ps = &proj->screens[indices[i]];
        ps->screen = packed[i];
        ps->screen.authored_bank = (uint8_t)bank;
        ps->generate_dirty = 0;
    }
    proj->chr_used[bank] = unique;
    return unique;
}
