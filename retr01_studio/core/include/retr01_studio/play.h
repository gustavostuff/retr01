#ifndef retr01_STUDIO_PLAY_H
#define retr01_STUDIO_PLAY_H

#include "retr01_studio/types.h"

/* Player AABB in world pixels (square for now). */
#define R01_PLAY_PLAYER_W 8
#define R01_PLAY_PLAYER_H 8
/* Default spawn: center of the spawn screen. */
#define R01_PLAY_SPAWN_CENTER_X(col) ((col)*R01_SCREEN_PX_W + (R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2)
#define R01_PLAY_SPAWN_CENTER_Y(row) ((row)*R01_SCREEN_PX_H + (R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2)

#define R01_PLAY_BTN_X 0
#define R01_PLAY_BTN_Y 1

typedef struct R01PlayState {
    int active;
    int player_x;
    int player_y;
    int cam_x;
    int cam_y;
} R01PlayState;

/* Viewport-relative OAM build (Studio Play / Phase D host). Entry 0 = player. */
typedef struct R01OamEntry {
    int x;
    int y;
    int bank;
    int tile_id;
    int pal;
    int flip_h;
    int flip_v;
} R01OamEntry;

int r01_play_start(R01PlayState *pl, const R01Project *p);
void r01_play_stop(R01PlayState *pl);
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);
int r01_play_button(R01PlayState *pl, const R01Project *p, int button);
int r01_play_screen_index(const R01PlayState *pl, const R01World *w);

int r01_play_sample_bg(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                       uint8_t *b);

/* Build OAM for current Play camera. Returns count written (incl. player as 0). */
int r01_play_build_oam(const R01Project *p, const R01PlayState *pl, R01OamEntry *out, int cap);

#endif
