#ifndef RETR01_STUDIO_PLAY_H
#define RETR01_STUDIO_PLAY_H

#include "retr01_studio/types.h"

#define R01_PLAY_PLAYER_SIZE 8

#define R01_PLAY_BTN_X 0
#define R01_PLAY_BTN_Y 1

typedef struct R01PlayState {
    int active;
    int player_x;
    int player_y;
    int cam_x;
    int cam_y;
} R01PlayState;

int r01_play_start(R01PlayState *pl, const R01Project *p);
void r01_play_stop(R01PlayState *pl);
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);
int r01_play_button(R01PlayState *pl, const R01Project *p, int button);

int r01_play_sample_bg(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                       uint8_t *b);

#endif
