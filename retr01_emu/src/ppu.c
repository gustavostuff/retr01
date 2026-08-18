#include "retr01_emu/ppu.h"

#include <string.h>

void retr01_ppu_init(retr01_ppu_t *ppu)
{
    memset(ppu, 0, sizeof(*ppu));
}

static retr01_rgb_t sample_bg(const retr01_ppu_t *ppu, int x, int y)
{
    int sx = (x + ppu->scroll_x) & 0xFF;
    int sy = (y + ppu->scroll_y) & 0xFF;
    int tx = sx / 8;
    int ty = sy / 8;
    int px = sx % 8;
    int py = sy % 8;
    uint8_t tile_idx;
    uint8_t pal_id;
    uint8_t master_idx;
    const uint8_t *tile;
    uint8_t plane0;
    uint8_t plane1;
    int bit;
    uint8_t ci;

    if (tx >= RETR01_NT_W || ty >= RETR01_NT_H) {
        retr01_rgb_t black = {0, 0, 0};
        return black;
    }

    tile_idx = ppu->screen.tiles[ty * RETR01_NT_W + tx];
    pal_id = retr01_attr_get(ppu->screen.attrs, tx, ty);

    {
        size_t bank_off =
            ((size_t)(ppu->world & 7) * 4u + (ppu->bg_bank & 3)) * 0x2000u + (size_t)tile_idx * 16u;
        if (!ppu->chr || ppu->chr_size < bank_off + 16) {
            bank_off = (size_t)tile_idx * 16u;
        }
        if (!ppu->chr || ppu->chr_size < bank_off + 16) {
            retr01_rgb_t magenta = {255, 0, 255};
            return magenta;
        }
        tile = ppu->chr + bank_off;
    }
    plane0 = tile[py];
    plane1 = tile[8 + py];
    bit = 7 - px;
    ci = (uint8_t)(((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1));

    if (ci == 0) {
        master_idx = ppu->palette.backdrop_index;
    } else {
        master_idx = ppu->palette.bg_palettes[pal_id & 3][ci & 3];
    }

    return ppu->palette.entries[master_idx];
}

void retr01_ppu_render_bg(const retr01_ppu_t *ppu, uint8_t *rgba_out)
{
    int y;
    int x;
    for (y = 0; y < RETR01_FB_H; y++) {
        for (x = 0; x < RETR01_FB_W; x++) {
            retr01_rgb_t c = sample_bg(ppu, x, y);
            size_t i = (size_t)(y * RETR01_FB_W + x) * 4;
            rgba_out[i + 0] = c.r;
            rgba_out[i + 1] = c.g;
            rgba_out[i + 2] = c.b;
            rgba_out[i + 3] = 255;
        }
    }
}
