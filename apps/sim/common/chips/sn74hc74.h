#ifndef R01A_SN74HC74_H
#define R01A_SN74HC74_H

#include "discrete_ic/entity.h"

/*
 * SN74HC74 dual D-type positive-edge-triggered flip-flop (DIP-14).
 * Stage 1: 1CLR# (pin 1), 1D (pin 2), 1CLK (pin 3), 1PRE# (pin 4), 1Q (pin 5), 1/Q (pin 6).
 * Stage 2: 2/Q (pin 8), 2Q (pin 9), 2PRE# (pin 10), 2CLK (pin 11), 2D (pin 12), 2CLR# (pin 13).
 * Pin 7: GND, Pin 14: VCC.
 */
typedef struct R01aSn74hc74 {
    NsEntity base;
    NsLevel clk1_prev;
    NsLevel clk2_prev;
    NsLevel q1;
    NsLevel q2;
} R01aSn74hc74;

void r01a_sn74hc74_init(R01aSn74hc74 *chip, const char *refdes);
NsEntity *r01a_sn74hc74_entity(R01aSn74hc74 *chip);

#endif
