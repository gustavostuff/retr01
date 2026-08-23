#ifndef RETR01_STUDIO_PLAY_H
#define RETR01_STUDIO_PLAY_H

#include "retr01_studio/types.h"

/* Flat Play player sprite (world pixels). */
#define R01_PLAY_PLAYER_SIZE 8

/* Default centered free box for DEADZONE / HYBRID scroll (viewport 128×120). */
#define R01_PLAY_DZ_FREE_W 32
#define R01_PLAY_DZ_FREE_H 30
#define R01_PLAY_DZ_INSET_X ((R01_SCREEN_PX_W - R01_PLAY_DZ_FREE_W) / 2) /* 48 */
#define R01_PLAY_DZ_INSET_Y ((R01_SCREEN_PX_H - R01_PLAY_DZ_FREE_H) / 2) /* 45 */

/* Pad buttons (cabinet mapping in Studio). */
#define R01_PLAY_BTN_X 0     /* keyboard Z */
#define R01_PLAY_BTN_Y 1     /* keyboard X */
#define R01_PLAY_BTN_COIN 2  /* keyboard Shift */
#define R01_PLAY_BTN_START 3 /* keyboard Enter */

typedef struct R01PlayState {
    int active;
    int player_x; /* world pixels — top-left of 8×8 player */
    int player_y;
    int cam_x;
    int cam_y;
    int frame;
    int fade; /* 0 = idle; >0 fading in/out ticks remaining */
    int home_col; /* HYBRID: screen the player is locked to */
    int home_row;
    int facing_dx; /* last move for HYBRID warp direction */
    int facing_dy;
} R01PlayState;

void r01_constraints_init_default(R01Constraints *c);

/* Effective constraints for active world (override or project). */
const R01Constraints *r01_project_constraints(const R01Project *p);

void r01_play_start(R01PlayState *pl, const R01Project *p);
void r01_play_stop(R01PlayState *pl);

/*
 * Apply arrow input (-1/0/1) and advance one tick.
 * Collision uses the full 8×8 AABB (all overlapped screen cells must be present).
 * HYBRID: player stays inside home screen; camera uses deadzone (no auto snap).
 */
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);

/*
 * Pad button press (edge-triggered from UI).
 * COIN / START in HYBRID: warp to a neighbor screen (facing / nearest edge),
 * player centered, camera snaps to that screen.
 * Returns 1 if something happened, 0 otherwise.
 */
int r01_play_button(R01PlayState *pl, const R01Project *p, int button);

/*
 * Sample one viewport pixel (0..127, 0..119) into kit RGB.
 * Applies BG ANIM frame from constraints.anim_rate when attr ANIM set.
 * Returns 0 ok, -1 if no coverage.
 */
int r01_play_sample(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                    uint8_t *b);

#endif
