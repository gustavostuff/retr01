#ifndef R01A_ATF22V10_H
#define R01A_ATF22V10_H

#include "discrete_ic/entity.h"

enum {
    R01A_PLD_BEAM_X = 0,
    R01A_PLD_BEAM_Y = 1
};

/*
 * ATF22V10 behavioral shell (not a fuse engine).
 * Beam X: DOT on CLK, count 0..340, HSYNC, Q0..Q8. Pin 14 is HSYNC.
 * Beam Y: HSYNC on CLK, count 0..261, VSYNC, Q0..Q8. Pin 14 is VSYNC.
 */
typedef struct R01aAtf22v10 {
    NsEntity base;
    int role;
    int x;
    int y;
    NsLevel clk_prev;
} R01aAtf22v10;

void r01a_atf22v10_init(R01aAtf22v10 *chip, const char *refdes, int role);
NsEntity *r01a_atf22v10_entity(R01aAtf22v10 *chip);
int r01a_atf22v10_x(const R01aAtf22v10 *chip);
int r01a_atf22v10_y(const R01aAtf22v10 *chip);
int r01a_atf22v10_hblank(const R01aAtf22v10 *chip);
int r01a_atf22v10_vblank(const R01aAtf22v10 *chip);

#endif
