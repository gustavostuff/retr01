#include "retr01_emu/video.h"

#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"

#include <stdlib.h>
#include <string.h>

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

static void world_bounds(const R01eCart *cart, const R01eWorldView *wv, int *min_c, int *min_r,
                         int *max_c, int *max_r);

void r01e_video_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b) {
    kit_rgb(master_index, r, g, b);
}

void r01e_video_reset(R01eVideo *vid) {
    if (!vid) {
        return;
    }
    memset(vid, 0, sizeof(*vid));
}

static void copy_pal_row(uint8_t *dst, const uint8_t *src16) {
    if (src16) {
        memcpy(dst, src16, R01E_PAL_ROW_BYTES);
    } else {
        memset(dst, 0, R01E_PAL_ROW_BYTES);
    }
}

/* Pick 16 B for row from a global plane (128 B preferred; 16 B = row 0 only). */
static const uint8_t *pal_row_ptr(const R01eCart *c, uint32_t off, uint32_t len, unsigned row) {
    const uint8_t *plane;
    unsigned r = row & 7u;
    uint32_t need;

    if (len == 0) {
        len = R01E_PAL_PLANE_BYTES;
    }
    if (len >= R01E_PAL_PLANE_BYTES) {
        need = R01E_PAL_PLANE_BYTES;
        plane = r01e_cart_ptr(c, off, need);
        if (plane) {
            return plane + (size_t)r * R01E_PAL_ROW_BYTES;
        }
    }
    plane = r01e_cart_ptr(c, off, R01E_PAL_ROW_BYTES);
    return plane; /* legacy single-row blob */
}

void r01e_video_load_active_pals(R01eMachine *m) {
    unsigned row;
    const uint8_t *pal_bg;
    const uint8_t *pal_spr;

    if (!m) {
        return;
    }
    row = m->io.pal_row & 7u;
    pal_bg = pal_row_ptr(&m->cart, m->cart.off_pal_bg, m->cart.len_pal_bg, row);
    pal_spr = pal_row_ptr(&m->cart, m->cart.off_pal_spr, m->cart.len_pal_spr, row);
    copy_pal_row(m->io.pal, pal_bg);
    copy_pal_row(m->io.pal + R01E_PAL_ROW_BYTES, pal_spr);
}

int r01e_video_softboot_enabled(void) {
    const char *e = getenv("R01E_SOFTBOOT");
    return e && e[0] != '\0' && e[0] != '0';
}

static int prepare_world_common(R01eMachine *m, int world, R01eWorldView *wv) {
    R01eVideo *vid;
    const uint8_t *chr;
    int si;
    int min_c, min_r, max_c, max_r;

    if (!m || !wv || world < 0 || world >= R01E_MAX_WORLDS) {
        return -1;
    }
    vid = &m->video;
    if (r01e_cart_world(&m->cart, world, wv) != 0) {
        return -1;
    }
    m->io.world = (uint8_t)world;

    chr = r01e_cart_ptr(&m->cart, wv->base + wv->off_chr, 8u * R01E_CHR_BANK_BYTES);
    if (!chr) {
        return -1;
    }
    for (si = 0; si < 8; si++) {
        memcpy(vid->chr[si], chr + (size_t)si * R01E_CHR_BANK_BYTES, R01E_CHR_BANK_BYTES);
    }
    vid->chr_loaded = 1;

    world_bounds(&m->cart, wv, &min_c, &min_r, &max_c, &max_r);
    vid->cam_origin_col = (int)wv->start_col;
    vid->cam_origin_row = (int)wv->start_row;
    vid->cam_x = vid->cam_origin_col * R01E_SCREEN_PX_W;
    vid->cam_y = vid->cam_origin_row * R01E_SCREEN_PX_H;
    vid->cam_max_x = max_c * R01E_SCREEN_PX_W;
    vid->cam_max_y = max_r * R01E_SCREEN_PX_H;
    if (vid->cam_max_x < vid->cam_x) {
        vid->cam_max_x = vid->cam_x;
    }
    if (vid->cam_max_y < vid->cam_y) {
        vid->cam_max_y = vid->cam_y;
    }
    /* L1 extent = present-screen bounding box (not the virtual 8x8). */
    vid->l1_cols = max_c - min_c + 1;
    vid->l1_rows = max_r - min_r + 1;
    if (vid->l1_cols < 1) {
        vid->l1_cols = 1;
    }
    if (vid->l1_rows < 1) {
        vid->l1_rows = 1;
    }
    vid->l1_origin_x = min_c * R01E_SCREEN_PX_W;
    vid->l1_origin_y = min_r * R01E_SCREEN_PX_H;
    return 0;
}

int r01e_video_prepare_world(R01eMachine *m, int world) {
    R01eWorldView wv;

    if (prepare_world_common(m, world, &wv) != 0) {
        return -1;
    }
    memset(m->video.vram, 0, sizeof(m->video.vram));
    memset(m->video.slot_present, 0, sizeof(m->video.slot_present));
    m->io.pal_row = (uint8_t)(wv.default_pal_row & 7u);
    r01e_video_load_bg0(m, &wv);
    /* Pals come from PRG $FE08/$FE09 stream (or softboot). */
    return 0;
}

int r01e_video_boot_world(R01eMachine *m, int world) {
    R01eWorldView wv;

    if (prepare_world_common(m, world, &wv) != 0) {
        return -1;
    }
    m->io.pal_row = (uint8_t)(wv.default_pal_row & 7u);
    r01e_video_load_active_pals(m);
    memset(m->video.vram, 0, sizeof(m->video.vram));
    memset(m->video.slot_present, 0, sizeof(m->video.slot_present));
    (void)r01e_video_sync_camera(m);
    r01e_video_load_parallax(m, &wv);
    return 0;
}

static void world_bounds(const R01eCart *cart, const R01eWorldView *wv, int *min_c, int *min_r,
                         int *max_c, int *max_r) {
    const uint8_t *dir;
    int si;

    *min_c = *min_r = 99;
    *max_c = *max_r = 0;
    dir = r01e_cart_ptr(cart, wv->base + wv->off_screen_dir, (size_t)wv->screen_count * 12u);
    if (!dir || wv->screen_count == 0) {
        *min_c = *min_r = *max_c = *max_r = 0;
        return;
    }
    for (si = 0; si < wv->screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        int c = (int)e[0];
        int r = (int)e[1];
        if (c < *min_c) {
            *min_c = c;
        }
        if (r < *min_r) {
            *min_r = r;
        }
        if (c > *max_c) {
            *max_c = c;
        }
        if (r > *max_r) {
            *max_r = r;
        }
    }
}

static int load_screen_into_slot(R01eMachine *m, const R01eWorldView *wv, int col, int row, int slot) {
    const uint8_t *dir;
    int si;
    uint8_t *dst = m->video.vram + (size_t)slot * R01E_VRAM_SLOT_BYTES;

    memset(dst, 0, R01E_VRAM_SLOT_BYTES);
    if (slot >= 0 && slot < 4) {
        m->video.slot_present[slot] = 0;
    }
    dir = r01e_cart_ptr(&m->cart, wv->base + wv->off_screen_dir, (size_t)wv->screen_count * 12u);
    if (!dir) {
        return 0;
    }
    for (si = 0; si < wv->screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        uint32_t poff;
        const uint8_t *pay;

        if ((int)e[0] != col || (int)e[1] != row) {
            continue;
        }
        poff = get_u24(e + 4);
        pay = r01e_cart_ptr(&m->cart, wv->base + poff, R01E_SCREEN_PAYLOAD);
        if (!pay) {
            return 0;
        }
        memcpy(dst, pay, R01E_SCREEN_PAYLOAD);
        if (slot >= 0 && slot < 4) {
            m->video.slot_present[slot] = 1;
        }
        return 1;
    }
    return 0;
}

static void bg0_update_scroll(R01eVideo *vid) {
    int rel_x, rel_y;
    if (!vid) {
        return;
    }
    /* Scroll relative to L1 present bbox origin, scaled by grid W/H. */
    rel_x = vid->cam_x - vid->l1_origin_x;
    rel_y = vid->cam_y - vid->l1_origin_y;
    if (rel_x < 0) {
        rel_x = 0;
    }
    if (rel_y < 0) {
        rel_y = 0;
    }
    if (vid->bg0_cols < 2 || vid->l1_cols < 1) {
        vid->l0_cam_x = 0;
    } else {
        vid->l0_cam_x = (rel_x * vid->bg0_cols) / vid->l1_cols;
    }
    if (vid->bg0_rows < 2 || vid->l1_rows < 1) {
        vid->l0_cam_y = 0;
    } else {
        vid->l0_cam_y = (rel_y * vid->bg0_rows) / vid->l1_rows;
    }
}

void r01e_video_update_bg0_scroll(R01eVideo *vid) {
    bg0_update_scroll(vid);
}

void r01e_video_load_bg0(R01eMachine *m, const R01eWorldView *wv) {
    R01eVideo *vid;
    const uint8_t *dir;
    int pi;
    int n;
    int min_c = 99, min_r = 99, max_c = 0, max_r = 0;

    if (!m || !wv) {
        return;
    }
    vid = &m->video;
    memset(vid->bg0, 0, sizeof(vid->bg0));
    vid->bg0_count = 0;
    vid->bg0_cols = 0;
    vid->bg0_rows = 0;
    vid->l0_cam_x = 0;
    vid->l0_cam_y = 0;
    if (wv->bg0_count == 0 || wv->off_bg0_dir == 0) {
        return;
    }
    n = (int)wv->bg0_count;
    if (n > R01E_PARALLAX_MAX) {
        n = R01E_PARALLAX_MAX;
    }
    dir = r01e_cart_ptr(&m->cart, wv->base + wv->off_bg0_dir, (size_t)n * 12u);
    if (!dir) {
        return;
    }
    for (pi = 0; pi < n; pi++) {
        const uint8_t *e = dir + (size_t)pi * 12u;
        uint32_t poff = get_u24(e + 4);
        const uint8_t *pay = r01e_cart_ptr(&m->cart, wv->base + poff, R01E_SCREEN_PAYLOAD);
        int c = (int)e[0];
        int r = (int)e[1];
        if (!pay) {
            continue;
        }
        vid->bg0[vid->bg0_count].present = 1;
        vid->bg0[vid->bg0_count].col = (uint8_t)c;
        vid->bg0[vid->bg0_count].row = (uint8_t)r;
        memcpy(vid->bg0[vid->bg0_count].map, pay, R01E_SCREEN_PAYLOAD);
        if (c < min_c) {
            min_c = c;
        }
        if (r < min_r) {
            min_r = r;
        }
        if (c > max_c) {
            max_c = c;
        }
        if (r > max_r) {
            max_r = r;
        }
        vid->bg0_count++;
    }
    if (vid->bg0_count > 0) {
        /*
         * Scroll ratio uses present-screen bounding box of L0 (same rule as L1),
         * not a larger authored slot grid with empty rows/cols.
         */
        vid->bg0_cols = max_c - min_c + 1;
        vid->bg0_rows = max_r - min_r + 1;
        if (vid->bg0_cols < 1) {
            vid->bg0_cols = 1;
        }
        if (vid->bg0_rows < 1) {
            vid->bg0_rows = 1;
        }
    }
    bg0_update_scroll(vid);
}

void r01e_video_load_parallax(R01eMachine *m, const R01eWorldView *wv) {
    r01e_video_load_bg0(m, wv);
}

void r01e_video_plane_slices_clear(R01eVideo *vid) {
    if (!vid) {
        return;
    }
    memset(vid->plane_h_slice, 0, sizeof(vid->plane_h_slice));
}

void r01e_video_plane_slice_set(R01eVideo *vid, int row, int8_t dx) {
    if (!vid || row < 0 || row >= R01E_PARALLAX_SLICE_MAX) {
        return;
    }
    vid->plane_h_slice[row] = dx;
}

int8_t r01e_video_plane_slice_get(const R01eVideo *vid, int row) {
    if (!vid || row < 0 || row >= R01E_PARALLAX_SLICE_MAX) {
        return 0;
    }
    return vid->plane_h_slice[row];
}

int r01e_video_sync_camera(R01eMachine *m) {
    R01eWorldView wv;
    R01eVideo *vid;
    int dx, dy;

    if (!m) {
        return -1;
    }
    vid = &m->video;
    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
        return -1;
    }
    for (dy = 0; dy < 2; dy++) {
        for (dx = 0; dx < 2; dx++) {
            (void)load_screen_into_slot(m, &wv, vid->cam_origin_col + dx, vid->cam_origin_row + dy,
                                        dy * 2 + dx);
        }
    }
    m->io.scroll_x = (uint8_t)(vid->cam_x - vid->cam_origin_col * R01E_SCREEN_PX_W);
    m->io.scroll_y = (uint8_t)(vid->cam_y - vid->cam_origin_row * R01E_SCREEN_PX_H);
    if (m->io.scroll_x > 127) {
        m->io.scroll_x = 127;
    }
    if (m->io.scroll_y > 119) {
        m->io.scroll_y = 119;
    }
    bg0_update_scroll(vid);
    return 0;
}

int r01e_video_host_pan(R01eMachine *m, int dx, int dy) {
    R01eVideo *vid;
    int nx, ny;

    if (!m || (dx == 0 && dy == 0)) {
        return 0;
    }
    vid = &m->video;
    nx = vid->cam_x + dx;
    ny = vid->cam_y + dy;
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (nx > vid->cam_max_x) {
        nx = vid->cam_max_x;
    }
    if (ny > vid->cam_max_y) {
        ny = vid->cam_max_y;
    }
    if (nx == vid->cam_x && ny == vid->cam_y) {
        return 0;
    }
    vid->cam_x = nx;
    vid->cam_y = ny;
    vid->cam_origin_col = nx / R01E_SCREEN_PX_W;
    vid->cam_origin_row = ny / R01E_SCREEN_PX_H;
    (void)r01e_video_sync_camera(m);
    r01e_video_render_frame(m);
    return 1;
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

static void backdrop_rgb(R01eMachine *m, uint8_t *r, uint8_t *g, uint8_t *b) {
    kit_rgb(m->io.pal[0] & 63u, r, g, b);
}

static void decode_tile16(uint8_t tile16[16], uint8_t attr) {
    int i;
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
}

static const R01eBg0Screen *bg0_find(const R01eVideo *vid, int col, int row) {
    int i;
    if (!vid) {
        return NULL;
    }
    for (i = 0; i < vid->bg0_count; i++) {
        if (vid->bg0[i].present && (int)vid->bg0[i].col == col && (int)vid->bg0[i].row == row) {
            return &vid->bg0[i];
        }
    }
    return NULL;
}

static void sample_l0(R01eMachine *m, int lx, int ly, uint8_t *r, uint8_t *g, uint8_t *b) {
    R01eVideo *vid = &m->video;
    int wx = vid->l0_cam_x + lx;
    int wy = vid->l0_cam_y + ly;
    int gc, gr, local_x, local_y, tx, ty, cell, px, py;
    const R01eBg0Screen *s;
    uint8_t tile, attr, bank, pal, col;
    const uint8_t *chr;
    uint8_t tile16[16];
    uint8_t master;

    if (vid->bg0_count < 1 || wx < 0 || wy < 0) {
        backdrop_rgb(m, r, g, b);
        return;
    }
    gc = wx / R01E_SCREEN_PX_W;
    gr = wy / R01E_SCREEN_PX_H;
    s = bg0_find(vid, gc, gr);
    if (!s) {
        backdrop_rgb(m, r, g, b);
        return;
    }
    local_x = wx - gc * R01E_SCREEN_PX_W;
    local_y = wy - gr * R01E_SCREEN_PX_H;
    tx = local_x / 8;
    ty = local_y / 8;
    cell = ty * R01E_SCREEN_TILES_X + tx;
    tile = s->map[cell];
    attr = s->map[0xF0 + cell];
    bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    chr = vid->chr[bank & 3u];
    memcpy(tile16, chr + (size_t)tile * R01E_TILE_BYTES, R01E_TILE_BYTES);
    decode_tile16(tile16, attr);
    px = local_x & 7;
    py = local_y & 7;
    col = tile_pix(tile16, px, py);
    if (col == 0) {
        backdrop_rgb(m, r, g, b);
        return;
    }
    master = m->io.pal[(pal & 3u) * 4u + (col & 3u)] & 63u;
    kit_rgb(master, r, g, b);
}

static void sample_bg(R01eMachine *m, int lx, int ly, uint8_t *r, uint8_t *g, uint8_t *b) {
    R01eVideo *vid = &m->video;
    int sx = m->io.scroll_x + lx;
    int sy = m->io.scroll_y + ly;
    int slot_x = sx / R01E_SCREEN_PX_W;
    int slot_y = sy / R01E_SCREEN_PX_H;
    int slot;
    int local_x, local_y, tx, ty, cell;
    const uint8_t *base;
    uint8_t tile, attr, bank, pal, col;
    const uint8_t *chr;
    uint8_t tile16[16];
    int px, py;
    uint8_t master;

    if (slot_x < 0 || slot_x > 1 || slot_y < 0 || slot_y > 1) {
        sample_l0(m, lx, ly, r, g, b);
        return;
    }
    slot = slot_y * 2 + slot_x;
    if (!vid->slot_present[slot]) {
        sample_l0(m, lx, ly, r, g, b);
        return;
    }
    local_x = sx - slot_x * R01E_SCREEN_PX_W;
    local_y = sy - slot_y * R01E_SCREEN_PX_H;
    tx = local_x / 8;
    ty = local_y / 8;
    cell = ty * R01E_SCREEN_TILES_X + tx;
    base = vid->vram + (size_t)slot * R01E_VRAM_SLOT_BYTES;
    tile = base[cell];
    attr = base[0xF0 + cell];
    bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    chr = vid->chr[bank & 3u];
    memcpy(tile16, chr + (size_t)tile * R01E_TILE_BYTES, R01E_TILE_BYTES);
    decode_tile16(tile16, attr);
    px = local_x & 7;
    py = local_y & 7;
    col = tile_pix(tile16, px, py);
    if (col == 0) {
        sample_l0(m, lx, ly, r, g, b);
        return;
    }
    master = m->io.pal[(pal & 3u) * 4u + (col & 3u)] & 63u;
    kit_rgb(master, r, g, b);
}

static void sample_vram_slot_px(R01eMachine *m, int slot, int local_x, int local_y, uint8_t *r, uint8_t *g,
                                uint8_t *b) {
    R01eVideo *vid = &m->video;
    int tx, ty, cell, px, py, i;
    const uint8_t *base;
    uint8_t tile, attr, bank, pal, col;
    const uint8_t *chr;
    uint8_t tile16[16];
    uint8_t master;

    backdrop_rgb(m, r, g, b);
    if (slot < 0 || slot > 3 || !vid->slot_present[slot]) {
        return;
    }
    if (local_x < 0 || local_y < 0 || local_x >= R01E_SCREEN_PX_W || local_y >= R01E_SCREEN_PX_H) {
        return;
    }
    tx = local_x / 8;
    ty = local_y / 8;
    cell = ty * R01E_SCREEN_TILES_X + tx;
    base = vid->vram + (size_t)slot * R01E_VRAM_SLOT_BYTES;
    tile = base[cell];
    attr = base[0xF0 + cell];
    bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    chr = vid->chr[bank & 3u];
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
            uint8_t t = tile16[i];
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
    master = m->io.pal[(pal & 3u) * 4u + (col & 3u)] & 63u;
    if (col == 0) {
        master = m->io.pal[0] & 63u;
    }
    kit_rgb(master, r, g, b);
}

static void composite_sprites_atlas(R01eMachine *m);

void r01e_video_render_vram_atlas(R01eMachine *m) {
    R01eVideo *vid;
    int ay, ax;

    if (!m) {
        return;
    }
    vid = &m->video;
    memset(vid->vram_atlas, 0, sizeof(vid->vram_atlas));
    if (!vid->chr_loaded) {
        return;
    }
    /* 2x2 slots -> 256x240 @ 1:1 (slot0 NW, 1 NE, 2 SW, 3 SE). */
    for (ay = 0; ay < R01E_VRAM_ATLAS_H; ay++) {
        for (ax = 0; ax < R01E_VRAM_ATLAS_W; ax++) {
            int slot_x = ax / R01E_SCREEN_PX_W;
            int slot_y = ay / R01E_SCREEN_PX_H;
            int slot = slot_y * 2 + slot_x;
            int lx = ax - slot_x * R01E_SCREEN_PX_W;
            int ly = ay - slot_y * R01E_SCREEN_PX_H;
            uint8_t r, g, b;
            size_t i = ((size_t)ay * R01E_VRAM_ATLAS_W + (size_t)ax) * 3u;
            sample_vram_slot_px(m, slot, lx, ly, &r, &g, &b);
            vid->vram_atlas[i] = r;
            vid->vram_atlas[i + 1] = g;
            vid->vram_atlas[i + 2] = b;
        }
    }
    composite_sprites_atlas(m);
}

static void flip_tile16(uint8_t tile16[16], uint8_t attr) {
    int i;
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
            uint8_t t = tile16[i];
            tile16[i] = tile16[7 - i];
            tile16[7 - i] = t;
            t = tile16[i + 8];
            tile16[i + 8] = tile16[15 - i];
            tile16[15 - i] = t;
        }
    }
}

static void put_fb_px2x(R01eVideo *vid, int lx, int ly, uint8_t r, uint8_t g, uint8_t b) {
    int ox, oy;
    if (lx < 0 || ly < 0 || lx >= R01E_SCREEN_PX_W || ly >= R01E_SCREEN_PX_H) {
        return;
    }
    for (oy = 0; oy < 2; oy++) {
        for (ox = 0; ox < 2; ox++) {
            int fx = lx * 2 + ox;
            int fy = ly * 2 + oy;
            size_t i = ((size_t)fy * R01E_VISIBLE_W + (size_t)fx) * 3u;
            vid->fb[i] = r;
            vid->fb[i + 1] = g;
            vid->fb[i + 2] = b;
        }
    }
}

static void put_atlas_px(R01eVideo *vid, int ax, int ay, uint8_t r, uint8_t g, uint8_t b) {
    size_t i;
    if (ax < 0 || ay < 0 || ax >= R01E_VRAM_ATLAS_W || ay >= R01E_VRAM_ATLAS_H) {
        return;
    }
    i = ((size_t)ay * R01E_VRAM_ATLAS_W + (size_t)ax) * 3u;
    vid->vram_atlas[i] = r;
    vid->vram_atlas[i + 1] = g;
    vid->vram_atlas[i + 2] = b;
}

static void blit_spr_tile(R01eMachine *m, int sx, int sy, uint8_t tile, uint8_t attr,
                          uint8_t line_count[R01E_SCREEN_PX_H]) {
    uint8_t bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    uint8_t pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    const uint8_t *chr = m->video.chr[4 + (bank & 3u)];
    uint8_t tile16[16];
    int px, py;

    memcpy(tile16, chr + (size_t)tile * R01E_TILE_BYTES, R01E_TILE_BYTES);
    flip_tile16(tile16, attr);
    for (py = 0; py < 8; py++) {
        int ly = sy + py;
        if (ly < 0 || ly >= R01E_SCREEN_PX_H) {
            continue;
        }
        if (line_count[ly] >= R01E_SPRITES_PER_LINE) {
            continue;
        }
        for (px = 0; px < 8; px++) {
            int lx = sx + px;
            uint8_t col;
            uint8_t master;
            uint8_t r, g, b;
            if (lx < 0 || lx >= R01E_SCREEN_PX_W) {
                continue;
            }
            col = tile_pix(tile16, px, py);
            if (col == 0) {
                continue;
            }
            master = m->io.pal[16u + (pal & 3u) * 4u + (col & 3u)] & 63u;
            kit_rgb(master, &r, &g, &b);
            put_fb_px2x(&m->video, lx, ly, r, g, b);
        }
        line_count[ly]++;
    }
}

static void blit_spr_tile_atlas(R01eMachine *m, int ax, int ay, uint8_t tile, uint8_t attr,
                                uint8_t line_count[R01E_VRAM_ATLAS_H]) {
    uint8_t bank = (uint8_t)(attr & R01E_ATTR_BANK_MASK);
    uint8_t pal = (uint8_t)((attr & R01E_ATTR_PAL_MASK) >> R01E_ATTR_PAL_SHIFT);
    const uint8_t *chr = m->video.chr[4 + (bank & 3u)];
    uint8_t tile16[16];
    int px, py;

    memcpy(tile16, chr + (size_t)tile * R01E_TILE_BYTES, R01E_TILE_BYTES);
    flip_tile16(tile16, attr);
    for (py = 0; py < 8; py++) {
        int row = ay + py;
        if (row < 0 || row >= R01E_VRAM_ATLAS_H) {
            continue;
        }
        if (line_count[row] >= R01E_SPRITES_PER_LINE) {
            continue;
        }
        for (px = 0; px < 8; px++) {
            int col = ax + px;
            uint8_t c;
            uint8_t master;
            uint8_t r, g, b;
            if (col < 0 || col >= R01E_VRAM_ATLAS_W) {
                continue;
            }
            c = tile_pix(tile16, px, py);
            if (c == 0) {
                continue;
            }
            master = m->io.pal[16u + (pal & 3u) * 4u + (c & 3u)] & 63u;
            kit_rgb(master, &r, &g, &b);
            put_atlas_px(&m->video, col, row, r, g, b);
        }
        line_count[row]++;
    }
}

static void composite_sprites(R01eMachine *m) {
    uint8_t line_count[R01E_SCREEN_PX_H];
    int ei;

    memset(line_count, 0, sizeof(line_count));
    for (ei = 0; ei < R01E_OAM_ENTRIES; ei++) {
        const uint8_t *e = &m->io.oam[(size_t)ei * R01E_OAM_ENTRY_BYTES];
        uint8_t sy_u = e[0];
        uint8_t tile = e[1];
        uint8_t attr = e[2];
        uint8_t sx_u = e[3];
        int sy = r01e_oam_coord_from_u8(sy_u);
        int sx = r01e_oam_coord_from_u8(sx_u);
        int tall = (attr & R01E_OAM_SIZE_16) != 0;

        if (tile == 0xFFu) {
            continue;
        }
        blit_spr_tile(m, sx, sy, tile, attr, line_count);
        if (tall) {
            blit_spr_tile(m, sx, sy + 8, (uint8_t)(tile | 1u), attr, line_count);
        }
    }
}

static void composite_sprites_atlas(R01eMachine *m) {
    R01eVideo *vid = &m->video;
    uint8_t line_count[R01E_VRAM_ATLAS_H];
    int origin_px = vid->cam_origin_col * R01E_SCREEN_PX_W;
    int origin_py = vid->cam_origin_row * R01E_SCREEN_PX_H;
    int ei;

    if (!(m->io.ctrl & R01E_PPUCTRL_BG_EN)) {
        return;
    }
    memset(line_count, 0, sizeof(line_count));
    for (ei = 0; ei < R01E_OAM_ENTRIES; ei++) {
        const uint8_t *e = &m->io.oam[(size_t)ei * R01E_OAM_ENTRY_BYTES];
        uint8_t sy_u = e[0];
        uint8_t tile = e[1];
        uint8_t attr = e[2];
        uint8_t sx_u = e[3];
        int sy = r01e_oam_coord_from_u8(sy_u);
        int sx = r01e_oam_coord_from_u8(sx_u);
        int ax = vid->cam_x + sx - origin_px;
        int ay = vid->cam_y + sy - origin_py;
        int tall = (attr & R01E_OAM_SIZE_16) != 0;

        if (tile == 0xFFu) {
            continue;
        }
        blit_spr_tile_atlas(m, ax, ay, tile, attr, line_count);
        if (tall) {
            blit_spr_tile_atlas(m, ax, ay + 8, (uint8_t)(tile | 1u), attr, line_count);
        }
    }
}

void r01e_video_render_frame(R01eMachine *m) {
    R01eVideo *vid;
    int lx, ly, ox, oy;

    if (!m) {
        return;
    }
    vid = &m->video;
    bg0_update_scroll(vid);
    if (!(m->io.ctrl & R01E_PPUCTRL_BG_EN) || !vid->chr_loaded) {
        memset(vid->fb, 0, sizeof(vid->fb));
        return;
    }
    for (ly = 0; ly < R01E_SCREEN_PX_H; ly++) {
        for (lx = 0; lx < R01E_SCREEN_PX_W; lx++) {
            uint8_t r, g, b;
            sample_bg(m, lx, ly, &r, &g, &b);
            for (oy = 0; oy < 2; oy++) {
                for (ox = 0; ox < 2; ox++) {
                    int fx = lx * 2 + ox;
                    int fy = ly * 2 + oy;
                    size_t i = ((size_t)fy * R01E_VISIBLE_W + (size_t)fx) * 3u;
                    vid->fb[i] = r;
                    vid->fb[i + 1] = g;
                    vid->fb[i + 2] = b;
                }
            }
        }
    }
    composite_sprites(m);
}
