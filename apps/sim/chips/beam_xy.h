#ifndef retr01_SIM_BEAM_XY_H
#define retr01_SIM_BEAM_XY_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_BEAM_DOTS_X 341
#define R01S_BEAM_DOTS_Y 262
#define R01S_BEAM_VISIBLE_W 256
#define R01S_BEAM_VISIBLE_H 240
#define R01S_HSYNC_FIRST 264
#define R01S_HSYNC_LAST 288
#define R01S_VSYNC_FIRST 243
#define R01S_VSYNC_LAST 245

/*
 * Beam X (UPLDX). Burned ATF22V10 image in docs/irl_bring_up_log/.
 * CLK is DOT, pin 1. Count 0..340. HSYNC on pin 14 is high on 264..288.
 * That pin clocks Beam Y. HBlank is X>=256, derived for the video path.
 */
typedef struct R01sBeamXy {
    R01sEntity base;
    int x;
    R01sLevel clk_prev;
} R01sBeamXy;

void r01s_beam_xy_init(R01sBeamXy *chip, const char *refdes);
R01sEntity *r01s_beam_xy_entity(R01sBeamXy *chip);
int r01s_beam_xy_x(const R01sBeamXy *chip);
int r01s_beam_xy_hblank(const R01sBeamXy *chip);
int r01s_beam_xy_hsync(const R01sBeamXy *chip);
void r01s_beam_xy_rewind(R01sBeamXy *chip);

static inline int r01s_beam_vblank_y(int y) {
    return y >= R01S_BEAM_VISIBLE_H;
}

#endif
