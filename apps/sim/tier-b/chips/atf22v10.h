#ifndef R01A_ATF22V10_H
#define R01A_ATF22V10_H

#include "discrete_ic/entity.h"

#include <stdint.h>

enum {
    R01A_PLD_BEAM_X = 0,
    R01A_PLD_BEAM_Y = 1,
    R01A_PLD_COMPOSITOR = 2
};

/*
 * ATF22V10 behavioral shell (not a fuse engine).
 * Beam X: DOT on CLK, X 0..340, X5/X6/X7, CSYNC/HSYNC, HBLANK, HWRAP.
 *         No color index (that moved to the Compositor in Tier B).
 * Beam Y: HWRAP on CLK, Y 0..261, Y5/Y6/Y7, VBLANK, VSYNC.
 * Compositor: DOT on CLK, 1-dot INDEX[5:0] latch, priority of test layers.
 *             PHI2 and RWB are safe stubs (equations ignore them).
 */
typedef struct R01aAtf22v10 {
    NsEntity base;
    int role;
    int x;
    int y;
    uint8_t index;
    NsLevel clk_prev;
} R01aAtf22v10;

void r01a_atf22v10_init(R01aAtf22v10 *chip, const char *refdes, int role);
NsEntity *r01a_atf22v10_entity(R01aAtf22v10 *chip);
int r01a_atf22v10_x(const R01aAtf22v10 *chip);
int r01a_atf22v10_y(const R01aAtf22v10 *chip);
int r01a_atf22v10_hblank(const R01aAtf22v10 *chip);
int r01a_atf22v10_vblank(const R01aAtf22v10 *chip);
uint8_t r01a_atf22v10_index(const R01aAtf22v10 *chip);

#endif
