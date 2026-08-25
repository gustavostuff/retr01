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
 * Host Play: Studio-equivalent move + camera + X/Y warps.
 * Sim: 1 logical px per sim VBlank when d-pad held. Scroll latched once per field.
 * Player renders via OAM on the beam (Island N/O).
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
    uint8_t pad_held;
    /* Latched on VBlank (hardware-accurate scroll / camera updates). */
    uint8_t video_pending;
    uint8_t pending_scroll_x;
    uint8_t pending_scroll_y;
    int pending_origin_col;
    int pending_origin_row;
    int pending_camera_reload;
    int force_camera_reload;
} R01sPlay;

void r01s_play_reset(R01sPlay *play);

/* Soft-load 2×2 camera + scroll from cart start / first present screen. */
int r01s_play_start(struct R01sBoard *board);

/* Sample P1 pad ($FE60 layout); movement runs on sim VBlank, not here. */
void r01s_play_tick(struct R01sBoard *board, uint8_t pad);

/* Overlay player onto LCD sink (after board video steps). */
/* Deprecated: player renders via OAM on the beam. Kept for API stability. */
void r01s_play_draw(struct R01sBoard *board);

/* VBlank: latch pending scroll / camera / OAM (from board_step). LCD field start handled by video_sink. */
void r01s_play_on_vblank(struct R01sBoard *board);

#endif
