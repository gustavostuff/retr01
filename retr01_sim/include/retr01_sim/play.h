#ifndef RETR01_SIM_PLAY_H
#define RETR01_SIM_PLAY_H

#include <stdint.h>

struct R01sBoard;

#define R01S_PLAY_PLAYER_SIZE 8

/* Player fill: sprite palette row 0, color index 1 (global_pal_spr[0].idx[1]). */
#define R01S_PAL_SPR_BASE 16
#define R01S_PAL_PLAYER_COLOR 1
#define R01S_ACTIVE_PAL_PLAYER (R01S_PAL_SPR_BASE + R01S_PAL_PLAYER_COLOR)

/*
 * Host Play — Studio/emu-equivalent move + camera + X/Y warps.
 * TEMPORARY / HIGH PRIORITY TO RETIRE with softboot: not IC behavior; soft-loads
 * 2×2 VRAM slots and pokes scroll latches. See docs/08_simulator.md.
 */
typedef struct R01sPlay {
    int enabled;
    int player_x;
    int player_y;
    int cam_x;
    int cam_y;
    int origin_col;
    int origin_row;
    int player_w;
    int player_h;
    uint8_t pad_prev;
} R01sPlay;

void r01s_play_reset(R01sPlay *play);

/* Soft-load 2×2 camera + scroll from cart start / first present screen. */
int r01s_play_start(struct R01sBoard *board);

/* One UI-frame tick from P1 pad bits ($FE60 layout). */
void r01s_play_tick(struct R01sBoard *board, uint8_t pad);

/* Overlay player onto LCD sink (after board video steps). */
void r01s_play_draw(struct R01sBoard *board);

#endif
