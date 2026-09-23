#ifndef R01A_OSC_DOT_H
#define R01A_OSC_DOT_H

#include "netlist_sim/entity.h"

/*
 * 5.369318 MHz DOT can oscillator (DIP-14 metal can, 4 legs used).
 * Pins: 14 VDD (pivot)  8 DOT  1 OE#  7 GND
 */
typedef struct R01aOscDot {
    NsEntity base;
} R01aOscDot;

void r01a_osc_dot_init(R01aOscDot *chip, const char *refdes);
NsEntity *r01a_osc_dot_entity(R01aOscDot *chip);

#endif
