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

/* Palette fade: 10 frames out (→ master 0) + 10 frames in. Kit-legal only. */
#define R01_PLAY_FADE_FRAMES 10
#define R01_PLAY_FADE_IDLE 0
#define R01_PLAY_FADE_OUT 1
#define R01_PLAY_FADE_IN 2

typedef struct R01PlayState {
    int active;
    int player_x; /* world pixels — top-left of 8×8 player */
    int player_y;
    int cam_x;
    int cam_y;
    int frame;
    int home_col; /* HYBRID: screen the player is locked to */
    int home_row;
    int facing_dx; /* last move for HYBRID warp direction */
    int facing_dy;
    /* Fade-to-black via BG+SPR palette slots (master indices → 0). */
    int fade_phase;  /* IDLE / OUT / IN */
    int fade_step;   /* 0..FADE_FRAMES within the current phase */
    int pending_col; /* screen to land on when all pal slots are 0 (-1 = none) */
    int pending_row;
    R01PalRow fade_bg[R01_PAL_ROWS];  /* live BG pal buffer (kit indices) */
    R01PalRow fade_spr[R01_PAL_ROWS]; /* live SPR pal buffer */
    R01PalRow fade_bg_full[R01_PAL_ROWS];  /* snapshot of authored pals */
    R01PalRow fade_spr_full[R01_PAL_ROWS];
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
 * Advances palette fade; swaps screen only when all BG+SPR fade slots are 0.
 */
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);

/*
 * Pad button press (edge-triggered from UI).
 * COIN / START in HYBRID: warp to a neighbor screen (facing / nearest edge).
 * With C8 FADE: starts fade-out; swap happens when pal slots hit master 0.
 * Returns 1 if something happened, 0 otherwise.
 */
int r01_play_button(R01PlayState *pl, const R01Project *p, int button);

/*
 * Sample one viewport pixel through the live fade BG palette (kit RGB).
 * Returns 0 ok, -1 if no coverage.
 */
int r01_play_sample(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                    uint8_t *b);

#endif
