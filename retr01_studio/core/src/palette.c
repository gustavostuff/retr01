#include "retr01_studio/palette.h"

#include "retr01_studio/chr_pack.h"

#include <string.h>

/* docs/02 kit swatches, row-major 16x4 */
static const uint8_t KIT_RGB[R01_MASTER_COLORS][3] = {
    {0x00, 0x00, 0x00}, {0x29, 0x05, 0x14}, {0x2A, 0x05, 0x07}, {0x23, 0x0F, 0x06},
    {0x1E, 0x13, 0x06}, {0x1A, 0x16, 0x05}, {0x14, 0x18, 0x07}, {0x06, 0x1A, 0x07},
    {0x05, 0x1A, 0x13}, {0x07, 0x19, 0x18}, {0x08, 0x18, 0x1C}, {0x07, 0x17, 0x22},
    {0x03, 0x0B, 0x3D}, {0x16, 0x03, 0x3A}, {0x20, 0x05, 0x2D}, {0x26, 0x04, 0x20},
    {0x36, 0x36, 0x36}, {0x74, 0x0A, 0x40}, {0x77, 0x09, 0x1A}, {0x69, 0x35, 0x12},
    {0x5D, 0x3F, 0x0E}, {0x51, 0x46, 0x17}, {0x42, 0x4C, 0x19}, {0x13, 0x51, 0x1A},
    {0x16, 0x50, 0x3F}, {0x11, 0x4E, 0x4D}, {0x16, 0x4D, 0x58}, {0x16, 0x4A, 0x66},
    {0x16, 0x37, 0x94}, {0x47, 0x29, 0x90}, {0x5F, 0x16, 0x7D}, {0x6C, 0x11, 0x5F},
    {0x94, 0x94, 0x94}, {0xC0, 0x4A, 0x7A}, {0xC5, 0x4A, 0x4D}, {0xB8, 0x60, 0x1B},
    {0xA2, 0x73, 0x26}, {0x8F, 0x7E, 0x2F}, {0x77, 0x87, 0x2D}, {0x20, 0x90, 0x30},
    {0x2E, 0x8E, 0x72}, {0x31, 0x8B, 0x89}, {0x1F, 0x88, 0x9C}, {0x24, 0x83, 0xB5},
    {0x4D, 0x77, 0xD7}, {0x7E, 0x6A, 0xD3}, {0x9D, 0x5D, 0xBF}, {0xB3, 0x52, 0xA0},
    {0xFF, 0xFF, 0xFF}, {0xF1, 0xA2, 0xBB}, {0xF1, 0xA6, 0xA1}, {0xF1, 0xA9, 0x83},
    {0xEE, 0xAC, 0x44}, {0xD4, 0xBA, 0x33}, {0xB0, 0xC8, 0x41}, {0x73, 0xD2, 0x75},
    {0x22, 0xD0, 0xA6}, {0x3B, 0xCD, 0xC9}, {0x48, 0xC9, 0xE4}, {0x88, 0xC4, 0xED},
    {0xA4, 0xBD, 0xEF}, {0xBB, 0xB5, 0xF1}, {0xD5, 0xA9, 0xEF}, {0xF0, 0x9B, 0xDD},
};

void r01_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = master_index & 63;
    if (r) {
        *r = KIT_RGB[i][0];
    }
    if (g) {
        *g = KIT_RGB[i][1];
    }
    if (b) {
        *b = KIT_RGB[i][2];
    }
}

int r01_nearest_kit_index(uint8_t r, uint8_t g, uint8_t b) {
    int best = 0;
    int best_d = 1 << 30;
    int i;
    for (i = 0; i < R01_MASTER_COLORS; i++) {
        int dr = (int)r - (int)KIT_RGB[i][0];
        int dg = (int)g - (int)KIT_RGB[i][1];
        int db = (int)b - (int)KIT_RGB[i][2];
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d) {
            best_d = d;
            best = i;
            if (d == 0) {
                break;
            }
        }
    }
    return best;
}

uint8_t r01_quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t rr = (uint8_t)((r * 7 + 127) / 255);
    uint8_t gg = (uint8_t)((g * 7 + 127) / 255);
    uint8_t bb = (uint8_t)((b * 3 + 127) / 255);
    return (uint8_t)((rr << 5) | (gg << 2) | bb);
}

void r01_r3g3b2_to_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t rr = (uint8_t)((packed >> 5) & 7u);
    uint8_t gg = (uint8_t)((packed >> 2) & 7u);
    uint8_t bb = (uint8_t)(packed & 3u);
    if (r) {
        *r = (uint8_t)((rr * 255) / 7);
    }
    if (g) {
        *g = (uint8_t)((gg * 255) / 7);
    }
    if (b) {
        *b = (uint8_t)((bb * 255) / 3);
    }
}

void r01_pal_row_init_default(R01PalRow *row, int row_index) {
    int base = (row_index & 3) * 16;
    if (!row) {
        return;
    }
    row->idx[0] = (uint8_t)(base + 0);
    row->idx[1] = (uint8_t)(base + 5);
    row->idx[2] = (uint8_t)(base + 10);
    row->idx[3] = (uint8_t)(base + 15);
}

void r01_project_init_default_pals(R01Project *p) {
    int i;
    if (!p) {
        return;
    }
    for (i = 0; i < R01_PAL_ROWS; i++) {
        r01_pal_row_init_default(&p->global_pal_bg[i], i);
        r01_pal_row_init_default(&p->global_pal_spr[i], i);
    }
}

const R01PalRow *r01_world_bg_pals(const R01Project *p, const R01World *w) {
    if (w && w->use_world_pals) {
        return w->pal_bg;
    }
    return p ? p->global_pal_bg : NULL;
}

const R01PalRow *r01_world_spr_pals(const R01Project *p, const R01World *w) {
    if (w && w->use_world_pals) {
        return w->pal_spr;
    }
    return p ? p->global_pal_spr : NULL;
}

void r01_tilemap_pixel_rgb(const R01Project *p, const R01World *w, const uint8_t *pixels, const uint8_t *attrs,
                           int px, int py, uint8_t *r, uint8_t *g, uint8_t *b) {
    int tx, ty, sx, sy, cell;
    uint8_t attr, color, master;
    const R01PalRow *rows;
    if (!pixels || !attrs || px < 0 || py < 0 || px >= R01_SCREEN_PX_W || py >= R01_SCREEN_PX_H) {
        if (r) {
            *r = 0;
        }
        if (g) {
            *g = 0;
        }
        if (b) {
            *b = 0;
        }
        return;
    }
    tx = px / 8;
    ty = py / 8;
    sx = px % 8;
    sy = py % 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    attr = attrs[cell];
    if (r01_attr_flip_h(attr)) {
        sx = 7 - sx;
    }
    if (r01_attr_flip_v(attr)) {
        sy = 7 - sy;
    }
    color = pixels[(ty * 8 + sy) * R01_SCREEN_PX_W + (tx * 8 + sx)] & 3u;
    rows = r01_world_bg_pals(p, w);
    master = rows ? rows[r01_attr_pal(attr)].idx[color] : color;
    r01_kit_rgb(master, r, g, b);
}

void r01_screen_pixel_rgb(const R01Project *p, const R01World *w, const R01Screen *s, int px, int py,
                          uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!s) {
        r01_tilemap_pixel_rgb(p, w, NULL, NULL, px, py, r, g, b);
        return;
    }
    r01_tilemap_pixel_rgb(p, w, s->pixels, s->attrs, px, py, r, g, b);
}

void r01_spr_chr_rgb(const R01Project *p, const R01World *w, int bank, int tile, uint8_t attr, int px, int py,
                     uint8_t *r, uint8_t *g, uint8_t *b, int *opaque) {
    uint8_t pix[64];
    int sx = px, sy = py;
    uint8_t color, master;
    const R01PalRow *rows;
    if (opaque) {
        *opaque = 0;
    }
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile < 0 || tile >= w->spr_banks[bank].tile_count) {
        return;
    }
    if (r01_attr_flip_h(attr)) {
        sx = 7 - sx;
    }
    if (r01_attr_flip_v(attr)) {
        sy = 7 - sy;
    }
    if (sx < 0 || sy < 0 || sx >= 8 || sy >= 8) {
        return;
    }
    r01_tile_to_pixels(&w->spr_banks[bank].chr[tile * R01_TILE_BYTES], pix);
    color = pix[sy * 8 + sx] & 3u;
    if (color == 0) {
        return;
    }
    rows = r01_world_spr_pals(p, w);
    master = rows ? rows[r01_attr_pal(attr)].idx[color] : color;
    r01_kit_rgb(master, r, g, b);
    if (opaque) {
        *opaque = 1;
    }
}
