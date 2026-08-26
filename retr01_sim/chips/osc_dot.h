#ifndef retr01_SIM_OSC_DOT_H
#define retr01_SIM_OSC_DOT_H

#include "retr01_sim/entity.h"

/*
 * Island H — ~5.369 MHz dot can oscillator (independent of PHI2).
 * Pins (DIP-8 half): 1 OE#  4 GND  5 DOT  8 VDD
 */
typedef struct R01sOscDot {
    R01sEntity base;
    int half_cycles;
} R01sOscDot;

void r01s_osc_dot_init(R01sOscDot *chip, const char *refdes);
R01sEntity *r01s_osc_dot_entity(R01sOscDot *chip);

#endif
