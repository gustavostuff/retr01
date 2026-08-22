#ifndef RETR01_STUDIO_PLAY_H
#define RETR01_STUDIO_PLAY_H

#include "retr01_studio/types.h"

typedef struct R01PlayState {
    int active;
    int player_x; /* world pixels */
    int player_y;
    int cam_x;
    int cam_y;
    int frame;
    int fade; /* 0 = idle; >0 fading in/out ticks remaining */
} R01PlayState;

void r01_constraints_init_default(R01Constraints *c);

/* Effective constraints for active world (override or project). */
const R01Constraints *r01_project_constraints(const R01Project *p);

void r01_play_start(R01PlayState *pl, const R01Project *p);
void r01_play_stop(R01PlayState *pl);

/*
 * Apply arrow input (-1/0/1) and advance one tick.
 * Updates player + camera from scroll mode; clamps to present screens.
 */
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);

/*
 * Sample one viewport pixel (0..127, 0..119) into kit RGB.
 * Applies BG ANIM frame from constraints.anim_rate when attr ANIM set.
 * Returns 0 ok, -1 if no coverage.
 */
int r01_play_sample(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                    uint8_t *b);

#endif
