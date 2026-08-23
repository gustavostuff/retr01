#include "retr01_emu/ppu.h"

#include "retr01_emu/machine.h"

#include <string.h>

/* docs/02 kit Color PROM (logical 24-bit). */
static const uint8_t KIT_RGB[R01E_MASTER_COLORS][3] = {
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

static uint32_t get_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static void kit_rgb(int idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = idx & 63;
    *r = KIT_RGB[i][0];
    *g = KIT_RGB[i][1];
    *b = KIT_RGB[i][2];
}

void r01e_ppu_reset(R01ePpu *ppu) {
    if (!ppu) {
        return;
    }
    memset(ppu, 0, sizeof(*ppu));
    ppu->ctrl = 0x01; /* BG on by default so soft-boot is visible */
}

static void copy_pal_rows(uint8_t *dst, const uint8_t *src16) {
    if (src16) {
        memcpy(dst, src16, 16);
    } else {
        memset(dst, 0, 16);
    }
}

int r01e_ppu_boot_world(R01eMachine *m, int world) {
    R01eWorldView wv;
    R01ePpu *ppu;
    const uint8_t *chr;
    const uint8_t *pal_bg;
    const uint8_t *pal_spr;
    int dx, dy, si, pi;
    if (!m || world < 0 || world >= R01E_MAX_WORLDS) {
        return -1;
    }
    ppu = &m->ppu;
    if (r01e_cart_world(&m->cart, world, &wv) != 0) {
        return -1;
    }
    ppu->world = (uint8_t)(world & 7);

    chr = r01e_cart_ptr(&m->cart, wv.base + wv.off_chr, 8u * R01E_CHR_BANK_BYTES);
    if (!chr) {
        return -1;
    }
    for (si = 0; si < 8; si++) {
        memcpy(ppu->chr[si], chr + (size_t)si * R01E_CHR_BANK_BYTES, R01E_CHR_BANK_BYTES);
    }
    ppu->chr_loaded = 1;

    if (wv.off_wpal_bg) {
        pal_bg = r01e_cart_ptr(&m->cart, wv.base + wv.off_wpal_bg, 16);
    } else {
        pal_bg = r01e_cart_ptr(&m->cart, m->cart.off_pal_bg, 16);
    }
    if (wv.off_wpal_spr) {
        pal_spr = r01e_cart_ptr(&m->cart, wv.base + wv.off_wpal_spr, 16);
    } else {
        pal_spr = r01e_cart_ptr(&m->cart, m->cart.off_pal_spr, 16);
    }
    copy_pal_rows(ppu->pal, pal_bg);
    copy_pal_rows(ppu->pal + 16, pal_spr);

    memset(ppu->vram, 0, sizeof(ppu->vram));

    /* Camera slots 0-3 around start_col/start_row. */
    for (dy = 0; dy < 2; dy++) {
        for (dx = 0; dx < 2; dx++) {
            int want_c = (int)wv.start_col + dx;
            int want_r = (int)wv.start_row + dy;
            int slot = dy * 2 + dx;
            const uint8_t *dir = r01e_cart_ptr(&m->cart, wv.base + wv.off_screen_dir,
                                               (size_t)wv.screen_count * 12u);
            if (!dir) {
                continue;
            }
            for (si = 0; si < wv.screen_count; si++) {
                const uint8_t *e = dir + (size_t)si * 12u;
                uint32_t poff;
                const uint8_t *pay;
                if ((int)e[0] != want_c || (int)e[1] != want_r) {
                    continue;
                }
                poff = get_u24(e + 4);
                pay = r01e_cart_ptr(&m->cart, wv.base + poff, R01E_SCREEN_PAYLOAD);
                if (!pay) {
                    break;
                }
                memcpy(ppu->vram + (size_t)slot * R01E_VRAM_SLOT_BYTES, pay, R01E_SCREEN_PAYLOAD);
                break;
            }
        }
    }

    /* Parallax -> slots 4-5. */
    {
        const uint8_t *pdir =
            r01e_cart_ptr(&m->cart, wv.base + wv.off_parallax_dir, (size_t)wv.parallax_count * 8u);
        if (pdir) {
            for (pi = 0; pi < wv.parallax_count; pi++) {
                const uint8_t *e = pdir + (size_t)pi * 8u;
                int pslot = (int)(e[0] & 1u);
                uint32_t poff = get_u24(e + 4);
                const uint8_t *pay = r01e_cart_ptr(&m->cart, wv.base + poff, R01E_SCREEN_PAYLOAD);
                int vslot = 4 + pslot;
                if (pay) {
                    memcpy(ppu->vram + (size_t)vslot * R01E_VRAM_SLOT_BYTES, pay, R01E_SCREEN_PAYLOAD);
                }
            }
        }
    }
    return 0;
}

uint8_t r01e_ppu_read(R01eMachine *m, uint16_t addr) {
    R01ePpu *ppu = &m->ppu;
    uint8_t v;
    switch (addr) {
    case 0xFE01:
        v = ppu->status;
        ppu->status = (uint8_t)(ppu->status & (uint8_t)~(R01E_PPUSTATUS_VBLANK | R01E_PPUSTATUS_HIT));
        return v;
    case 0xFE02:
        return ppu->scroll_x;
    case 0xFE03:
        return ppu->scroll_y;
    case 0xFE04:
        return ppu->raster_y;
    case 0xFE05:
        return ppu->raster_ctrl;
    case 0xFE09:
        v = ppu->pal[ppu->pal_addr & 31u];
        ppu->pal_addr = (uint8_t)((ppu->pal_addr + 1) & 31u);
        return v;
    case 0xFE12:
        v = ppu->vram[ppu->vram_addr & (R01E_VRAM_BYTES - 1)];
        ppu->vram_addr = (uint16_t)((ppu->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        return v;
    case 0xFE21:
        v = ppu->oam[ppu->oam_addr];
        ppu->oam_addr++;
        return v;
    case 0xFE30:
        return ppu->world;
    case 0xFE38:
        return ppu->pal_row;
    case 0xFE60:
        return ppu->pad0;
    case 0xFE61:
        return ppu->pad1;
    case 0xFE93:
        v = r01e_cart_read(&m->cart, ppu->map_addr);
        ppu->map_addr = (ppu->map_addr + 1) & 0xFFFFFFu;
        return v;
    default:
        if (addr >= 0xFE40 && addr <= 0xFE5F) {
            return ppu->apu[addr - 0xFE40];
        }
        if (addr >= 0xFE31 && addr <= 0xFE37) {
            return ppu->bank_helper[addr - 0xFE30];
        }
        if (addr >= 0xFE70 && addr <= 0xFE72) {
            return ppu->eeprom[addr - 0xFE70];
        }
        return 0;
    }
}

void r01e_ppu_write(R01eMachine *m, uint16_t addr, uint8_t v) {
    R01ePpu *ppu = &m->ppu;
    switch (addr) {
    case 0xFE00:
        ppu->ctrl = v;
        break;
    case 0xFE02:
        ppu->scroll_x = (uint8_t)(v & 127u);
        break;
    case 0xFE03:
        ppu->scroll_y = (uint8_t)(v < 120u ? v : 119u);
        break;
    case 0xFE04:
        ppu->raster_y = v;
        break;
    case 0xFE05:
        ppu->raster_ctrl = v;
        break;
    case 0xFE06:
        ppu->plane_lo = v;
        break;
    case 0xFE07:
        ppu->plane_hi = v;
        break;
    case 0xFE08:
        ppu->pal_addr = (uint8_t)(v & 31u);
        break;
    case 0xFE09:
        ppu->pal[ppu->pal_addr & 31u] = (uint8_t)(v & 63u);
        ppu->pal_addr = (uint8_t)((ppu->pal_addr + 1) & 31u);
        break;
    case 0xFE10:
        ppu->vram_addr = (uint16_t)((ppu->vram_addr & 0xFF00u) | v);
        break;
    case 0xFE11:
        ppu->vram_addr = (uint16_t)((ppu->vram_addr & 0x00FFu) | ((uint16_t)v << 8));
        ppu->vram_addr = (uint16_t)(ppu->vram_addr & (R01E_VRAM_BYTES - 1));
        break;
    case 0xFE12:
        ppu->vram[ppu->vram_addr & (R01E_VRAM_BYTES - 1)] = v;
        ppu->vram_addr = (uint16_t)((ppu->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        break;
    case 0xFE20:
        ppu->oam_addr = v;
        break;
    case 0xFE21:
        ppu->oam[ppu->oam_addr++] = v;
        break;
    case 0xFE30:
        ppu->world = (uint8_t)(v & 7u);
        (void)r01e_ppu_boot_world(m, (int)ppu->world);
        break;
    case 0xFE38:
        ppu->pal_row = (uint8_t)(v & 3u);
        break;
    case 0xFE60:
        /* CPU write ignored; pads are host-driven. */
        break;
    case 0xFE61:
        break;
    case 0xFE90:
        ppu->map_addr = (ppu->map_addr & 0xFFFF00u) | v;
        break;
    case 0xFE91:
        ppu->map_addr = (ppu->map_addr & 0xFF00FFu) | ((uint32_t)v << 8);
        break;
    case 0xFE92:
        ppu->map_addr = (ppu->map_addr & 0x00FFFFu) | ((uint32_t)v << 16);
        break;
    case 0xFE93:
        /* MAP data port is read-auto-inc; writes ignored for now. */
        (void)v;
        break;
    default:
        if (addr >= 0xFE40 && addr <= 0xFE5F) {
            ppu->apu[addr - 0xFE40] = v;
        } else if (addr >= 0xFE31 && addr <= 0xFE37) {
            ppu->bank_helper[addr - 0xFE30] = v;
        } else if (addr >= 0xFE70 && addr <= 0xFE72) {
            ppu->eeprom[addr - 0xFE70] = v;
        }
        break;
    }
}

static uint8_t tile_pix(const uint8_t tile16[16], int px, int py) {
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

static void sample_bg(R01ePpu *ppu, int lx, int ly, uint8_t *r, uint8_t *g, uint8_t *b) {
    int sx = ppu->scroll_x + lx;
    int sy = ppu->scroll_y + ly;
    int slot_x = sx / R01E_SCREEN_PX_W;
    int slot_y = sy / R01E_SCREEN_PX_H;
    int slot;
    int local_x, local_y, tx, ty, cell;
    const uint8_t *base;
    uint8_t tile, attr, bank, pal, col;
    const uint8_t *chr;
    uint8_t tile16[16];
    int px, py, i;
    uint8_t master;

    if (slot_x < 0 || slot_x > 1 || slot_y < 0 || slot_y > 1) {
        kit_rgb(0, r, g, b);
        return;
    }
    slot = slot_y * 2 + slot_x;
    local_x = sx - slot_x * R01E_SCREEN_PX_W;
    local_y = sy - slot_y * R01E_SCREEN_PX_H;
    tx = local_x / 8;
    ty = local_y / 8;
    cell = ty * R01E_SCREEN_TILES_X + tx;
    base = ppu->vram + (size_t)slot * R01E_VRAM_SLOT_BYTES;
    tile = base[cell];
    attr = base[0xF0 + cell];
    bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    chr = ppu->chr[bank & 3u];
    memcpy(tile16, chr + (size_t)tile * R01E_TILE_BYTES, R01E_TILE_BYTES);
    if (attr & R01E_ATTR_FLIP_H) {
        for (i = 0; i < 8; i++) {
            uint8_t v0 = tile16[i], v1 = tile16[i + 8], o0 = 0, o1 = 0;
            int bit;
            for (bit = 0; bit < 8; bit++) {
                if (v0 & (1u << bit)) {
                    o0 |= (uint8_t)(1u << (7 - bit));
                }
                if (v1 & (1u << bit)) {
                    o1 |= (uint8_t)(1u << (7 - bit));
                }
            }
            tile16[i] = o0;
            tile16[i + 8] = o1;
        }
    }
    if (attr & R01E_ATTR_FLIP_V) {
        for (i = 0; i < 4; i++) {
            uint8_t t;
            t = tile16[i];
            tile16[i] = tile16[7 - i];
            tile16[7 - i] = t;
            t = tile16[i + 8];
            tile16[i + 8] = tile16[15 - i];
            tile16[15 - i] = t;
        }
    }
    px = local_x & 7;
    py = local_y & 7;
    col = tile_pix(tile16, px, py);
    master = ppu->pal[(pal & 3u) * 4u + (col & 3u)] & 63u;
    if (col == 0) {
        master = ppu->pal[0] & 63u; /* shared color 0 */
    }
    kit_rgb(master, r, g, b);
}

void r01e_ppu_render_frame(R01eMachine *m) {
    R01ePpu *ppu;
    int lx, ly, ox, oy;
    if (!m) {
        return;
    }
    ppu = &m->ppu;
    if (!(ppu->ctrl & 0x01u) || !ppu->chr_loaded) {
        memset(ppu->fb, 0, sizeof(ppu->fb));
        return;
    }
    for (ly = 0; ly < R01E_SCREEN_PX_H; ly++) {
        for (lx = 0; lx < R01E_SCREEN_PX_W; lx++) {
            uint8_t r, g, b;
            sample_bg(ppu, lx, ly, &r, &g, &b);
            /* SCALE 2x into 256x240 */
            for (oy = 0; oy < 2; oy++) {
                for (ox = 0; ox < 2; ox++) {
                    int fx = lx * 2 + ox;
                    int fy = ly * 2 + oy;
                    size_t i = ((size_t)fy * R01E_VISIBLE_W + (size_t)fx) * 3u;
                    ppu->fb[i] = r;
                    ppu->fb[i + 1] = g;
                    ppu->fb[i + 2] = b;
                }
            }
        }
    }
}

void r01e_ppu_dot(R01eMachine *m) {
    R01ePpu *ppu = &m->ppu;
    int entered_vblank = 0;

    ppu->dot_x++;
    if (ppu->dot_x >= R01E_DOTS_X) {
        ppu->dot_x = 0;
        ppu->dot_y++;
        if (ppu->dot_y >= R01E_DOTS_Y) {
            ppu->dot_y = 0;
            ppu->frame++;
            r01e_ppu_render_frame(m);
        }
        if (ppu->dot_y == R01E_VISIBLE_H) {
            entered_vblank = 1;
        }
    }

    if (entered_vblank) {
        ppu->status |= R01E_PPUSTATUS_VBLANK;
        if (ppu->ctrl & 0x80u) {
            m->nmi_pending = 1;
        }
    }
    if (ppu->dot_y == 0 && ppu->dot_x == 0) {
        ppu->status = (uint8_t)(ppu->status & (uint8_t)~R01E_PPUSTATUS_VBLANK);
    }
}
