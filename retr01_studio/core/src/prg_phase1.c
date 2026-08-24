#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include <string.h>

#define CODE_BASE 0x8000u
#define PLAY_OFF 0x0100u /* PRG+$0100 → CPU $8100 */

/* CPU $8100 play table (shared Studio ↔ emu contract) */
#define PLAY_PRESENT 0 /* 8 bytes: bit col in row */
#define PLAY_SPAWN_C 8
#define PLAY_SPAWN_R 9

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
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

static void pick_spawn(const R01World *w, int *col, int *row) {
    int i;
    *col = R01_START_COL;
    *row = R01_START_ROW;
    if (w && r01_world_find_screen(w, R01_START_COL, R01_START_ROW) >= 0) {
        return;
    }
    if (!w) {
        return;
    }
    for (i = 0; i < w->screen_count; i++) {
        if (w->screens[i].present) {
            *col = w->screens[i].col;
            *row = w->screens[i].row;
            return;
        }
    }
}

void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01World *w) {
    uint8_t mask[8];
    int spawn_c = R01_START_COL, spawn_r = R01_START_ROW;
    uint16_t hang;

    /*
     * Boot + VBlank pad poll. Gameplay SoT is Studio play.c — Phase 1 runners
     * (Studio Play / emu cart runtime) apply that algorithm to cart MAP + this
     * table. Marker R01P lets the emu enable the matching runtime.
     */
    static const uint8_t code[] = {
        /* reset */
        0x78,             /* SEI */
        0xD8,             /* CLD */
        0xA2, 0xFF,       /* LDX #$FF */
        0x9A,             /* TXS */
        0xA9, 0x00,       /* LDA #0 */
        0x8D, 0x30, 0xFE, /* STA $FE30 WORLD */
        0x8D, 0x02, 0xFE, /* STA $FE02 SCROLL_X */
        0x8D, 0x03, 0xFE, /* STA $FE03 SCROLL_Y */
        0xA9, 0x01,       /* LDA #1 */
        0x8D, 0x00, 0xFE, /* STA $FE00 PPUCTRL (BG on) */
        /* main: wait VBlank */
        /* $8014: */
        0xAD, 0x01, 0xFE, /* LDA $FE01 */
        0x29, 0x80,       /* AND #$80 */
        0xF0, 0xF9,       /* BEQ $8014 */
        0xAD, 0x60, 0xFE, /* LDA $FE60 */
        0x8D, 0xFE, 0x00, /* STA $00FE (pad snapshot) */
        0x4C, 0x14, 0x80, /* JMP $8014 */
    };

    memset(prg, 0xEA, R01_PRG_BYTES);
    memcpy(prg, code, sizeof(code));

    fill_present_mask(mask, w);
    pick_spawn(w, &spawn_c, &spawn_r);
    memcpy(prg + PLAY_OFF + PLAY_PRESENT, mask, 8);
    prg[PLAY_OFF + PLAY_SPAWN_C] = (uint8_t)spawn_c;
    prg[PLAY_OFF + PLAY_SPAWN_R] = (uint8_t)spawn_r;

    /* Marker at PRG+$00F0 → CPU $80F0 */
    prg[0x00F0] = 'R';
    prg[0x00F1] = '0';
    prg[0x00F2] = '1';
    prg[0x00F3] = 'P';
    prg[0x00F4] = 1;

    hang = 0x8014u;
    put_u16_le(prg + 0x7FFA, hang);
    put_u16_le(prg + 0x7FFC, CODE_BASE);
    put_u16_le(prg + 0x7FFE, hang);
}
