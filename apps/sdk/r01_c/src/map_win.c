#include "r01_engine.h"

#include "r01_cart_caps.h"

#ifndef R01_HOST_TEST

#define R01_VRAM_SLOT 512u
#define R01_SCREEN_DIR 12u
#define R01_GRID_CELLS 256u

static uint32_t s_pay[R01_GRID_CELLS];
static uint8_t s_origin_col = 0xFFu;
static uint8_t s_origin_row = 0xFFu;
static uint8_t s_slot_col[4];
static uint8_t s_slot_row[4];
static uint8_t s_slot_has[4];
static uint8_t s_loaded;
static uint8_t s_n0x;
static uint8_t s_n0y;
static uint8_t s_n1x = 1;
static uint8_t s_n1y = 1;
static uint8_t s_minc;
static uint8_t s_minr;
static uint16_t s_bg0_cx = 0xFFFFu;
static uint16_t s_bg0_cy = 0xFFFFu;
static uint8_t s_bg0_wx = 0xFFu;
static uint8_t s_bg0_wy = 0xFFu;
static uint16_t s_play = 0x8100u;
static uint8_t s_map_cache[R01_SCREEN_PAYLOAD];
static uint8_t s_map_col = 0xFFu;
static uint8_t s_map_row = 0xFFu;
static uint8_t s_map_ok;

static void vram_seek(uint16_t addr) {
    *R01_VRAM_ADDR_LO = (uint8_t)(addr & 0xFFu);
    *R01_VRAM_ADDR_HI = (uint8_t)(addr >> 8);
}

static void copy_payload(uint32_t off, uint16_t vram_addr) {
    r01_map_seek(off);
    vram_seek(vram_addr);
    r01_vram_copy_map();
}

static void fill_zero(uint16_t vram_addr) {
    vram_seek(vram_addr);
    r01_vram_fill_zero();
}

static uint8_t y_to_row(uint16_t y, uint8_t *ly) {
    uint8_t row = 0;
    while (y >= (uint16_t)R01_SCREEN_PX_H) {
        y -= (uint16_t)R01_SCREEN_PX_H;
        row++;
        if (row >= 16u) {
            break;
        }
    }
    if (ly) {
        *ly = (uint8_t)y;
    }
    return row;
}

static void cache_l1_extents(void) {
    uint8_t minc = 15;
    uint8_t maxc = 0;
    uint8_t minr = 15;
    uint8_t maxr = 0;
    uint8_t found = 0;
    uint8_t r;
    uint8_t c;
    for (r = 0; r < 16u; r++) {
        uint16_t bits = *(volatile uint8_t *)(uint16_t)(s_play + (uint16_t)r * 2u);
        bits |= (uint16_t)(*(volatile uint8_t *)(uint16_t)(s_play + (uint16_t)r * 2u + 1u)) << 8;
        if (bits == 0u) {
            continue;
        }
        found = 1;
        if (r < minr) {
            minr = r;
        }
        if (r > maxr) {
            maxr = r;
        }
        for (c = 0; c < 16u; c++) {
            if ((bits & (uint16_t)(1u << c)) == 0u) {
                continue;
            }
            if (c < minc) {
                minc = c;
            }
            if (c > maxc) {
                maxc = c;
            }
        }
    }
    if (!found) {
        s_minc = 0;
        s_minr = 0;
        s_n1x = 1;
        s_n1y = 1;
        return;
    }
    s_minc = minc;
    s_minr = minr;
    s_n1x = (uint8_t)(maxc - minc + 1u);
    s_n1y = (uint8_t)(maxr - minr + 1u);
}

void r01_world_enter(uint8_t id) {
    uint32_t world0;
    uint32_t wtable;
    uint32_t world;
    uint8_t n;
    uint8_t i;
    uint8_t hdr3;
    uint8_t present;
    uint32_t dir;
    uint16_t cell;
    uint16_t ptr;

    if (id >= 7u) {
        id = 0;
    }
    ptr = (uint16_t)(*(volatile uint8_t *)(uint16_t)(0x8500u + (uint16_t)id * 2u));
    ptr |= (uint16_t)(*(volatile uint8_t *)(uint16_t)(0x8500u + (uint16_t)id * 2u + 1u)) << 8;
    if (ptr == 0u) {
        ptr = 0x8100u;
    }
    s_play = ptr;

    for (cell = 0; cell < R01_GRID_CELLS; cell++) {
        s_pay[cell] = 0;
    }
    s_origin_col = 0xFFu;
    s_origin_row = 0xFFu;
    s_loaded = 0;
    s_map_ok = 0;
    s_map_col = 0xFFu;
    s_map_row = 0xFFu;
    for (i = 0; i < 4u; i++) {
        s_slot_has[i] = 0;
        s_slot_col[i] = 0xFFu;
        s_slot_row[i] = 0xFFu;
    }
    s_n0x = 0;
    s_n0y = 0;
    s_bg0_cx = 0xFFFFu;
    s_bg0_cy = 0xFFFFu;
    s_bg0_wx = 0xFFu;
    s_bg0_wy = 0xFFu;
    cache_l1_extents();

    world0 = r01_boot_u24(12);
    if (world0 == 0u) {
        return;
    }
    wtable = world0 - (uint32_t)(7u * 8u);
    r01_map_seek(wtable + (uint32_t)id * 8u);
    present = r01_map_read();
    if (present == 0u) {
        if (id != 0u) {
            r01_world_enter(0);
        }
        return;
    }
    (void)r01_map_read();
    world = r01_map_read_u24();
    if (world == 0u) {
        return;
    }
    r01_map_seek(world + 3u);
    hdr3 = r01_map_read();
    s_n0x = (uint8_t)(hdr3 & 0x0Fu);
    s_n0y = (uint8_t)((hdr3 >> 4) & 0x0Fu);
    r01_map_seek(world + 5u);
    n = r01_map_read();
    r01_map_seek(world + 11u);
    dir = world + r01_map_read_u24();
    for (i = 0; i < n; i++) {
        uint32_t e = dir + (uint32_t)i * R01_SCREEN_DIR;
        uint8_t packed;
        uint32_t poff;
        int col;
        int row;
        r01_map_seek(e);
        packed = r01_map_read();
        r01_map_seek(e + 4u);
        poff = r01_map_read_u24();
        col = R01_CELL_COL(packed);
        row = R01_CELL_ROW(packed);
        if (col < 0 || col > 15 || row < 0 || row > 15) {
            continue;
        }
        s_pay[(uint16_t)row * 16u + (uint16_t)col] = world + poff;
    }
}

void r01_world_cache_boot(void) {
    r01_world_enter(*R01_WORLD_PORT);
}

uint16_t r01_play_base(void) {
    return s_play;
}

int r01_map_tile_at(uint8_t col, uint8_t row, uint8_t cell, uint8_t *tile, uint8_t *attr) {
    uint32_t pay;
    uint16_t i;
    if (col > 15u || row > 15u || cell >= (uint8_t)R01_TILES_PER_SCREEN) {
        return -1;
    }
    pay = s_pay[(uint16_t)row * 16u + (uint16_t)col];
    if (pay == 0u) {
        return -1;
    }
    if (!s_map_ok || s_map_col != col || s_map_row != row) {
        r01_map_lock();
        r01_map_seek(pay);
        for (i = 0; i < (uint16_t)R01_SCREEN_PAYLOAD; i++) {
            s_map_cache[i] = r01_map_read();
        }
        r01_map_unlock();
        s_map_col = col;
        s_map_row = row;
        s_map_ok = 1;
    }
    if (tile) {
        *tile = s_map_cache[cell];
    }
    if (attr) {
        *attr = s_map_cache[(uint16_t)R01_TILES_PER_SCREEN + (uint16_t)cell];
    }
    return 0;
}

void r01_map_load_window(uint16_t cam_x, uint16_t cam_y) {
    uint8_t ox = (uint8_t)(cam_x >> 7);
    uint8_t oy = y_to_row(cam_y, 0);
    uint8_t dy;
    uint8_t dx;
    if (s_loaded && ox == s_origin_col && oy == s_origin_row) {
        return;
    }
    s_origin_col = ox;
    s_origin_row = oy;
    for (dy = 0; dy < 2u; dy++) {
        for (dx = 0; dx < 2u; dx++) {
            uint8_t col = (uint8_t)(ox + dx);
            uint8_t row = (uint8_t)(oy + dy);
            uint8_t slot = (uint8_t)(dy * 2u + dx);
            uint16_t addr = (uint16_t)slot * (uint16_t)R01_VRAM_SLOT;
            uint32_t pay = 0;
            if (col < 16u && row < 16u) {
                pay = s_pay[(uint16_t)row * 16u + (uint16_t)col];
            }
            if (pay) {
                if (!s_slot_has[slot] || s_slot_col[slot] != col || s_slot_row[slot] != row) {
                    copy_payload(pay, addr);
                }
                s_slot_has[slot] = 1;
                s_slot_col[slot] = col;
                s_slot_row[slot] = row;
            } else if (s_slot_has[slot] || !s_loaded) {
                fill_zero(addr);
                s_slot_has[slot] = 0;
                s_slot_col[slot] = 0xFFu;
                s_slot_row[slot] = 0xFFu;
            }
        }
    }
    s_loaded = 1;
}

static uint8_t bg0_axis(uint16_t rel, uint8_t n0, uint8_t n1, uint8_t wrap) {
    uint16_t num;
    uint8_t den;
    if (n0 < 2u || n1 < 2u || n0 >= n1) {
        return 0;
    }
    if (wrap) {
        num = rel * (uint16_t)n0;
        return (uint8_t)((num / (uint16_t)n1) & 127u);
    }
    den = (uint8_t)(n1 - 1u);
    if (den == 0u) {
        return 0;
    }
    num = rel * (uint16_t)(n0 - 1u) + (uint16_t)(den / 2u);
    return (uint8_t)((num / (uint16_t)den) & 127u);
}

void r01_bg0_publish(const R01GameCtx *ctx) {
    uint16_t relx;
    uint16_t rely;
    uint16_t minx;
    uint16_t miny;
    if (!ctx) {
        return;
    }
    if (ctx->cam_x == s_bg0_cx && ctx->cam_y == s_bg0_cy && ctx->bg0_wrap_x == s_bg0_wx &&
        ctx->bg0_wrap_y == s_bg0_wy) {
        return;
    }
    s_bg0_cx = ctx->cam_x;
    s_bg0_cy = ctx->cam_y;
    s_bg0_wx = ctx->bg0_wrap_x;
    s_bg0_wy = ctx->bg0_wrap_y;
    minx = (uint16_t)s_minc << 7;
    miny = (uint16_t)s_minr * (uint16_t)R01_SCREEN_PX_H;
    relx = 0;
    rely = 0;
    if (ctx->cam_x >= minx) {
        relx = (uint16_t)(ctx->cam_x - minx);
    }
    if (ctx->cam_y >= miny) {
        rely = (uint16_t)(ctx->cam_y - miny);
    }
    *R01_BG0_SCROLL_X = bg0_axis(relx, s_n0x, s_n1x, ctx->bg0_wrap_x);
    *R01_BG0_SCROLL_Y = bg0_axis(rely, s_n0y, s_n1y, ctx->bg0_wrap_y);
}

#else
void r01_world_cache_boot(void) {
}

void r01_world_enter(uint8_t world_id) {
    (void)world_id;
}

uint16_t r01_play_base(void) {
    return 0x8100u;
}

int r01_map_tile_at(uint8_t col, uint8_t row, uint8_t cell, uint8_t *tile, uint8_t *attr) {
    (void)col;
    (void)row;
    (void)cell;
    if (tile) {
        *tile = 0;
    }
    if (attr) {
        *attr = 0;
    }
    return 0;
}

void r01_map_load_window(uint16_t cam_x, uint16_t cam_y) {
    (void)cam_x;
    (void)cam_y;
}

void r01_bg0_publish(const R01GameCtx *ctx) {
    (void)ctx;
}
#endif
