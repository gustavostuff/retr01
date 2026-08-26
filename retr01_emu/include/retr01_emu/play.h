#ifndef RETR01_EMU_PLAY_H
#define RETR01_EMU_PLAY_H

#include <stdint.h>

struct R01eMachine;

#define R01E_PLAY_PLAYER_SIZE 8
#define R01E_START_COL 1
#define R01E_START_ROW 1

/* Player fill: sprite row color index 1 in active buffer (io.pal[16+1]). */
#define R01E_PAL_SPR_BASE 16
#define R01E_PAL_PLAYER_COLOR 1
#define R01E_PAL_PLAYER (R01E_PAL_SPR_BASE + R01E_PAL_PLAYER_COLOR)

typedef enum R01eEventKind {
    R01E_EVT_NONE = 0,
    R01E_EVT_WARP_SCREEN,
    R01E_EVT_FADE_BLACK,
} R01eEventKind;

typedef struct R01eEvent {
    R01eEventKind kind;
    int arg0;
    int arg1;
} R01eEvent;

typedef struct R01ePlay {
    int enabled;
    int player_x;
    int player_y;
    int cam_x;
    int cam_y;
    int player_w;
    int player_h;
    uint8_t pad_prev;
    int entity_count; /* phase 4+ */
} R01ePlay;

void r01e_play_reset(R01ePlay *play);
int r01e_play_start(struct R01eMachine *m);
void r01e_play_tick(struct R01eMachine *m);
void r01e_play_draw(struct R01eMachine *m);
void r01e_play_player_rgb(const struct R01eMachine *m, uint8_t *r, uint8_t *g, uint8_t *b);

/* Mirror play camera into video scroll / 2x2 workbench. */
void r01e_play_sync_video(struct R01eMachine *m);
void r01e_play_post_event(R01ePlay *play, R01eEvent evt);

#endif
