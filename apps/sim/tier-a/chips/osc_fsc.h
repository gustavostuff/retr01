#ifndef R01A_OSC_FSC_H
#define R01A_OSC_FSC_H

#include "netlist_sim/entity.h"

/*
 * 3.579545 MHz NTSC FSC can into AD724 FIN.
 * Pins (DIP-8 half): 1 OE#  4 GND  5 FSC  8 VDD
 */
typedef struct R01aOscFsc {
    NsEntity base;
} R01aOscFsc;

void r01a_osc_fsc_init(R01aOscFsc *chip, const char *refdes);
NsEntity *r01a_osc_fsc_entity(R01aOscFsc *chip);

#endif
