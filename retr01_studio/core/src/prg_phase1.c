#include "retr01_studio/collision.h"
#include "retr01_studio/play.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include "play_collision_bin.h"

#include <string.h>

#define CODE_BASE 0x8000u
#define PLAY_OFF 0x0100u /* PRG+$0100 → CPU $8100 */

#define PLAY_PRESENT 0
#define PLAY_SPAWN_C 8
#define PLAY_SPAWN_R 9
#define PLAY_COLL_COUNT 10
#define PLAY_COLL_DIR 11 /* 4 bytes/screen: col, row, tab_lo, tab_hi (CPU addr) */

#define R01P_OFF 0x00F0u
#define R01P_VER_COLLISION 2u
#define R01_PLAY_COLLISION_OFF 0x0500u /* CPU $8500 */
#define R01_PLAY_SOLID_DATA_OFF 0x0700u /* CPU $8700 — solid shadow tables */

enum {
    PRG_OFF_BG_LO = 1,
    PRG_OFF_BG_MID = 6,
    PRG_OFF_BG_HI = 11,
    PRG_OFF_SPR_LO = 32,
    PRG_OFF_SPR_MID = 37,
    PRG_OFF_SPR_HI = 42,
    PRG_OFF_MAP_LO = 58,
    PRG_OFF_MAP_MID = 63,
    PRG_OFF_MAP_HI = 68,
};

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint32_t pal_row_off(uint32_t plane_off, uint32_t plane_len, unsigned row) {
    unsigned r = row & 7u;
    if (plane_len == 0 || plane_len >= R01_PAL_PLANE_BYTES) {
        return plane_off + r * 16u;
    }
    return plane_off;
}

static void fill_present_mask(uint8_t mask[8], const R01World *w) {
    int i;
    memset(mask, 0, 8);
    if (!w) {
        return;
    }
    for (i = 0; i < w->screen_count; i++) {
        const R01Screen *s = &w->screens[i];
        if (!s->present || s->col < 0 || s->col > 7 || s->row < 0 || s->row > 7) {
            continue;
        }
        mask[s->row] = (uint8_t)(mask[s->row] | (uint8_t)(1u << (unsigned)s->col));
    }
}

static void pick_spawn_scroll(const R01World *w, int col, int row, uint8_t *out_sx, uint8_t *out_sy) {
    int px;
    int py;
    int ax;
    int ay;
    int cam_x;
    int cam_y;
    int ox;
    int oy;
    uint8_t sx;
    uint8_t sy;

    (void)w;
    px = R01_PLAY_SPAWN_CENTER_X(col);
    py = R01_PLAY_SPAWN_CENTER_Y(row);
    ax = px + R01_PLAY_PLAYER_W / 2;
    ay = py + R01_PLAY_PLAYER_H / 2;
    cam_x = ax - R01_SCREEN_PX_W / 2;
    cam_y = ay - R01_SCREEN_PX_H / 2;
    if (cam_x < 0) {
        cam_x = 0;
    }
    if (cam_y < 0) {
        cam_y = 0;
    }
    ox = cam_x / R01_SCREEN_PX_W;
    oy = cam_y / R01_SCREEN_PX_H;
    sx = (uint8_t)(cam_x - ox * R01_SCREEN_PX_W);
    sy = (uint8_t)(cam_y - oy * R01_SCREEN_PX_H);
    if (sx > 127u) {
        sx = 127u;
    }
    if (sy > 119u) {
        sy = 119u;
    }
    *out_sx = sx;
    *out_sy = sy;
}

static void pick_spawn(const R01World *w, int *col, int *row) {
    int idx;
    if (!col || !row) {
        return;
    }
    *col = R01_START_COL;
    *row = R01_START_ROW;
    if (!w) {
        return;
    }
    idx = r01_world_default_screen(w);
    if (idx >= 0 && idx < w->screen_count && w->screens[idx].present) {
        *col = w->screens[idx].col;
        *row = w->screens[idx].row;
    }
}

static size_t append_boot_stream(uint8_t *out, const R01PrgCartLayout *layout) {
    static const uint8_t stream[] = {
        /* BG palette row (16 master indices) */
        0xA9, 0x00,       /* LDA #bg_lo */
        0x8D, 0x90, 0xFE, /* STA $FE90 */
        0xA9, 0x00,       /* LDA #bg_mid */
        0x8D, 0x91, 0xFE, /* STA $FE91 */
        0xA9, 0x00,       /* LDA #bg_hi */
        0x8D, 0x92, 0xFE, /* STA $FE92 */
        0xA9, 0x00,       /* LDA #$00 */
        0x8D, 0x08, 0xFE, /* STA $FE08 */
        0xA2, 0x10,       /* LDX #16 */
        0xAD, 0x93, 0xFE, /* LDA $FE93 */
        0x8D, 0x09, 0xFE, /* STA $FE09 */
        0xCA,             /* DEX */
        0xD0, 0xF7,       /* BNE *-9 */
        /* SPR palette row */
        0xA9, 0x00,       /* LDA #spr_lo */
        0x8D, 0x90, 0xFE, /* STA $FE90 */
        0xA9, 0x00,       /* LDA #spr_mid */
        0x8D, 0x91, 0xFE, /* STA $FE91 */
        0xA9, 0x00,       /* LDA #spr_hi */
        0x8D, 0x92, 0xFE, /* STA $FE92 */
        0xA2, 0x10,       /* LDX #16 */
        0xAD, 0x93, 0xFE, /* LDA $FE93 */
        0x8D, 0x09, 0xFE, /* STA $FE09 */
        0xCA,             /* DEX */
        0xD0, 0xF7,       /* BNE *-9 */
        /* Start-screen MAP -> VRAM */
        0xA9, 0x00,       /* LDA #map_lo */
        0x8D, 0x90, 0xFE, /* STA $FE90 */
        0xA9, 0x00,       /* LDA #map_mid */
        0x8D, 0x91, 0xFE, /* STA $FE91 */
        0xA9, 0x00,       /* LDA #map_hi */
        0x8D, 0x92, 0xFE, /* STA $FE92 */
        0xA9, 0x00,       /* LDA #$00 */
        0x8D, 0x10, 0xFE, /* STA $FE10 */
        0x8D, 0x11, 0xFE, /* STA $FE11 */
        0xA2, 0xF0,       /* LDX #240 */
        0xAD, 0x93, 0xFE, /* LDA $FE93 */
        0x8D, 0x12, 0xFE, /* STA $FE12 */
        0xCA,             /* DEX */
        0xD0, 0xF7,       /* BNE *-9 */
        0xA2, 0xF0,       /* LDX #240 */
        0xAD, 0x93, 0xFE, /* LDA $FE93 */
        0x8D, 0x12, 0xFE, /* STA $FE12 */
        0xCA,             /* DEX */
        0xD0, 0xF7,       /* BNE *-9 */
    };
    uint32_t off_bg;
    uint32_t off_spr;

    memcpy(out, stream, sizeof(stream));
    if (!layout || layout->off_map_screen0 == 0 || layout->off_pal_bg == 0) {
        return sizeof(stream);
    }
    off_bg = pal_row_off(layout->off_pal_bg, layout->len_pal_bg, layout->default_pal_row);
    off_spr = layout->off_pal_spr
                  ? pal_row_off(layout->off_pal_spr, layout->len_pal_spr, layout->default_pal_row)
                  : off_bg + 16u;
    out[PRG_OFF_BG_LO] = (uint8_t)(off_bg & 0xFFu);
    out[PRG_OFF_BG_MID] = (uint8_t)((off_bg >> 8) & 0xFFu);
    out[PRG_OFF_BG_HI] = (uint8_t)((off_bg >> 16) & 0xFFu);
    out[PRG_OFF_SPR_LO] = (uint8_t)(off_spr & 0xFFu);
    out[PRG_OFF_SPR_MID] = (uint8_t)((off_spr >> 8) & 0xFFu);
    out[PRG_OFF_SPR_HI] = (uint8_t)((off_spr >> 16) & 0xFFu);
    out[PRG_OFF_MAP_LO] = (uint8_t)(layout->off_map_screen0 & 0xFFu);
    out[PRG_OFF_MAP_MID] = (uint8_t)((layout->off_map_screen0 >> 8) & 0xFFu);
    out[PRG_OFF_MAP_HI] = (uint8_t)((layout->off_map_screen0 >> 16) & 0xFFu);
    return sizeof(stream);
}

static void fill_collision_tables(uint8_t prg[R01_PRG_BYTES], const R01World *w) {
    size_t data_off = R01_PLAY_SOLID_DATA_OFF;
    int di = 0;
    int si;

    if (!w) {
        prg[PLAY_OFF + PLAY_COLL_COUNT] = 0;
        return;
    }
    for (si = 0; si < w->screen_count; si++) {
        const R01Screen *s = &w->screens[si];
        int cell;
        uint16_t tab_addr;
        uint8_t *ent;
        if (!s->present || s->col < 0 || s->col > 7 || s->row < 0 || s->row > 7) {
            continue;
        }
        if (data_off + R01_TILES_PER_SCREEN > R01_PRG_BYTES) {
            break;
        }
        tab_addr = (uint16_t)(CODE_BASE + data_off);
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            prg[data_off++] = (s->attrs[cell] & R01_ATTR_SOLID) ? 1u : 0u;
        }
        if (PLAY_OFF + PLAY_COLL_DIR + (size_t)(di + 1) * 4u > R01_PLAY_COLLISION_OFF) {
            break;
        }
        ent = prg + PLAY_OFF + PLAY_COLL_DIR + (size_t)di * 4u;
        ent[0] = (uint8_t)s->col;
        ent[1] = (uint8_t)s->row;
        ent[2] = (uint8_t)(tab_addr & 0xFFu);
        ent[3] = (uint8_t)(tab_addr >> 8);
        di++;
    }
    prg[PLAY_OFF + PLAY_COLL_COUNT] = (uint8_t)di;
}

static void install_collision_code(uint8_t prg[R01_PRG_BYTES]) {
    if (R01_PLAY_COLLISION_OFF + play_collision_bin_len > R01_PLAY_SOLID_DATA_OFF) {
        return;
    }
    memcpy(prg + R01_PLAY_COLLISION_OFF, play_collision_bin, play_collision_bin_len);
}

void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01World *w, const R01PrgCartLayout *layout) {
    uint8_t mask[8];
    int spawn_c = R01_START_COL, spawn_r = R01_START_ROW;
    size_t n = 0;
    uint16_t main_pc;
    static const uint8_t init[] = {
        0x78,             /* SEI */
        0xD8,             /* CLD */
        0xA2, 0xFF,       /* LDX #$FF */
        0x9A,             /* TXS */
        0xA9, 0x00,       /* LDA #0 */
        0x8D, 0x30, 0xFE, /* STA $FE30 WORLD */
        0xA9, 0x00,       /* LDA #scroll_x (R01_PRG_INIT_SCROLL_X) */
        0x8D, 0x02, 0xFE, /* STA $FE02 SCROLL_X */
        0xA9, 0x00,       /* LDA #scroll_y (R01_PRG_INIT_SCROLL_Y) */
        0x8D, 0x03, 0xFE, /* STA $FE03 SCROLL_Y */
        0xA9, 0x01,       /* LDA #1 */
        0x8D, 0x00, 0xFE, /* STA $FE00 PPUCTRL (BG on) */
    };
    uint8_t scroll_x = 0;
    uint8_t scroll_y = 0;
    static const uint8_t main_loop[] = {
        /* main: wait VBlank */
        0xAD, 0x01, 0xFE, /* LDA $FE01 */
        0x29, 0x80,       /* AND #$80 */
        0xF0, 0xF9,       /* BEQ main */
        0xAD, 0x60, 0xFE, /* LDA $FE60 */
        0x8D, 0xFE, 0x00, /* STA $00FE (pad snapshot) */
        0x4C, 0x00, 0x80, /* JMP main (patched) */
    };

    memset(prg, 0xEA, R01_PRG_BYTES);
    pick_spawn(w, &spawn_c, &spawn_r);
    pick_spawn_scroll(w, spawn_c, spawn_r, &scroll_x, &scroll_y);
    memcpy(prg + n, init, sizeof(init));
    prg[n + R01_PRG_INIT_SCROLL_X] = scroll_x;
    prg[n + R01_PRG_INIT_SCROLL_Y] = scroll_y;
    n += sizeof(init);
    n += append_boot_stream(prg + n, layout);
    main_pc = (uint16_t)(CODE_BASE + n);
    memcpy(prg + n, main_loop, sizeof(main_loop));
    /* Patch JMP abs operand (last 2 bytes), not the LDA $FE60 in the middle. */
    prg[n + sizeof(main_loop) - 2] = (uint8_t)(main_pc & 0xFFu);
    prg[n + sizeof(main_loop) - 1] = (uint8_t)(main_pc >> 8);
    n += sizeof(main_loop);

    fill_present_mask(mask, w);
    memcpy(prg + PLAY_OFF + PLAY_PRESENT, mask, 8);
    prg[PLAY_OFF + PLAY_SPAWN_C] = (uint8_t)spawn_c;
    prg[PLAY_OFF + PLAY_SPAWN_R] = (uint8_t)spawn_r;

    install_collision_code(prg);
    fill_collision_tables(prg, w);

    prg[R01P_OFF] = 'R';
    prg[R01P_OFF + 1] = '0';
    prg[R01P_OFF + 2] = '1';
    prg[R01P_OFF + 3] = 'P';
    prg[R01P_OFF + 4] = R01P_VER_COLLISION;
    put_u16_le(prg + R01P_OFF + 5, (uint16_t)(CODE_BASE + R01_PLAY_COLLISION_OFF));

    put_u16_le(prg + 0x7FFA, main_pc);
    put_u16_le(prg + 0x7FFC, CODE_BASE);
    put_u16_le(prg + 0x7FFE, main_pc);
}
