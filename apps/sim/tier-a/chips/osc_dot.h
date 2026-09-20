#ifndef R01A_OSC_DOT_H
#define R01A_OSC_DOT_H

#include "netlist_sim/entity.h"

/*
 * 5.369318 MHz DOT can oscillator.
 * Pins (DIP-8 half): 1 OE#  4 GND  5 DOT  8 VDD
 */
typedef struct R01aOscDot {
    NsEntity base;
} R01aOscDot;

void r01a_osc_dot_init(R01aOscDot *chip, const char *refdes);
NsEntity *r01a_osc_dot_entity(R01aOscDot *chip);

#endif
