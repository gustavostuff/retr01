#ifndef RETR01_SIM_BEAM_XY_H
#define RETR01_SIM_BEAM_XY_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_BEAM_DOTS_X 341
#define R01S_BEAM_DOTS_Y 262
#define R01S_BEAM_VISIBLE_W 256
#define R01S_BEAM_VISIBLE_H 240

/*
 * Island H — behavioral stand-in for ATF22V10 X/Y beam PLDs (32-IC BOM).
 * Rising DOT advances X; wrap 341→0 bumps Y; wrap 262→0.
 * HBLANK = X>=256, VBLANK = Y>=240, NMI# pulses low on VBlank entry.
 */
typedef struct R01sBeamXy {
    R01sEntity base;
    int x;
    int y;
    R01sLevel dot_prev;
    int nmi_hold; /* dots to hold NMI# low after VBlank entry */
} R01sBeamXy;

void r01s_beam_xy_init(R01sBeamXy *chip, const char *refdes);
R01sEntity *r01s_beam_xy_entity(R01sBeamXy *chip);
int r01s_beam_xy_x(const R01sBeamXy *chip);
int r01s_beam_xy_y(const R01sBeamXy *chip);
int r01s_beam_xy_hblank(const R01sBeamXy *chip);
int r01s_beam_xy_vblank(const R01sBeamXy *chip);

#endif
