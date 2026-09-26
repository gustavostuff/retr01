#include "s1_lab.h"

#include <stdio.h>
#include <string.h>

#ifndef R01C_EXAMPLE01_CHR
#define R01C_EXAMPLE01_CHR "../../../example_01/data/chr_spr0.bin"
#endif

#define R01C_TILE_BYTES 16u
#define R01C_CHR_BYTES 4096u

/* example_01 global_pal_spr row 0 (player) and row 1 (slime). */
static const uint8_t PLAYER_KIT[4] = {0, 18, 33, 51};
static const uint8_t SLIME_KIT[4] = {0, 2, 17, 51};

typedef struct R01cPart {
    uint8_t tile;
    int8_t dx;
    int8_t dy;
} R01cPart;

/* From example_01.r01proj player running frames 0 and 1 (origin 16,16). */
static const R01cPart PLAYER_RUN_A[] = {
    {0, 8, 4}, {1, 16, 4}, {12, 8, 12}, {13, 16, 12}, {14, 8, 20}, {15, 16, 20},
};
static const R01cPart PLAYER_RUN_B[] = {
    {6, 8, 4}, {7, 16, 4}, {16, 8, 12}, {17, 16, 12}, {18, 8, 20}, {19, 16, 20},
};

static uint8_t g_chr[R01C_CHR_BYTES];
static int g_chr_loaded;

static int load_chr_once(void) {
    FILE *f;
    size_t n;
    if (g_chr_loaded) {
        return 1;
    }
    f = fopen(R01C_EXAMPLE01_CHR, "rb");
    if (!f) {
        return 0;
    }
    n = fread(g_chr, 1, sizeof(g_chr), f);
    fclose(f);
    if (n < 28u * R01C_TILE_BYTES) {
        return 0;
    }
    g_chr_loaded = 1;
    return 1;
}

static uint8_t tile_pix_2bpp(const uint8_t *tile16, int px, int py) {
    int bit = 7 - (px & 7);
    uint8_t p0 = tile16[py & 7];
    uint8_t p1 = tile16[(py & 7) + 8];
    uint8_t c = 0;
    if (p0 & (1u << bit)) {
        c |= 1u;
    }
    if (p1 & (1u << bit)) {
        c |= 2u;
    }
    return c;
}

static void blit_tile(R01aAs6c62256 *field, int dest_x, int dest_y, uint8_t tile_id,
                      const uint8_t kit_map[4]) {
    const uint8_t *tile16;
    int row;
    int col;
    if (!field || !field->mem || tile_id >= 128u) {
        return;
    }
    if (!load_chr_once()) {
        return;
    }
    tile16 = g_chr + (size_t)tile_id * R01C_TILE_BYTES;
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            int x = dest_x + col;
            int y = dest_y + row;
            uint8_t px;
            uint8_t kit;
            if (x < 0 || x >= R01A_FIELD_W || y < 0 || y >= R01A_FIELD_H) {
                continue;
            }
            px = tile_pix_2bpp(tile16, col, row);
            if (px == 0) {
                continue;
            }
            kit = kit_map[px & 3u];
            if (kit == 0) {
                continue;
            }
            field->mem[(size_t)y * (size_t)R01A_FIELD_W + (size_t)x] = kit;
        }
    }
}

static void blit_player_frame(R01aAs6c62256 *field, int anchor_x, int anchor_y, int origin_x,
                              int origin_y, const R01cPart *parts, size_t n_parts) {
    size_t i;
    for (i = 0; i < n_parts; i++) {
        int tx = anchor_x + (int)parts[i].dx - origin_x;
        int ty = anchor_y + (int)parts[i].dy - origin_y;
        blit_tile(field, tx, ty, parts[i].tile, PLAYER_KIT);
    }
}

static void field_clear(R01aAs6c62256 *field) {
    if (!field || !field->mem) {
        return;
    }
    memset(field->mem, 0, R01A_FIELD_BYTES);
}

void r01c_s1_vblank_field(R01aAs6c62256 *field, uint32_t frame) {
    const R01cPart *run;
    if (!field) {
        return;
    }
    field_clear(field);
    run = ((frame / 8u) & 1u) ? PLAYER_RUN_B : PLAYER_RUN_A;
    blit_player_frame(field, 84, 56, 16, 16, run, sizeof(PLAYER_RUN_A) / sizeof(PLAYER_RUN_A[0]));
    blit_tile(field, 20, 96, 28, SLIME_KIT);
}

void r01c_s1_hblank_bg0_line(R01aAs6c62256 *field, int line_y, int ping) {
    uint32_t base;
    int x;
    if (!field || !field->mem || line_y < 0 || line_y >= R01A_FIELD_H) {
        return;
    }
    (void)ping;
    base = R01C_BG0_LINE_BASE + (uint32_t)line_y * (uint32_t)R01A_FIELD_W;
    for (x = 0; x < R01A_FIELD_W; x++) {
        uint8_t k = (((x + line_y) / 16) & 1) ? 14u : 18u;
        if (line_y > 90) {
            k = 0;
        }
        field->mem[base + (uint32_t)x] = k;
    }
}
