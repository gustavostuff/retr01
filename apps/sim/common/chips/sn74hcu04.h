#ifndef R01A_SN74HCU04_H
#define R01A_SN74HCU04_H

#include "discrete_ic/entity.h"

/*
 * SN74HCU04 hex unbuffered inverter (DIP-14).
 * Gate 1: Y2 Pierce oscillator amplifier (1A pin 1, 1Y pin 2).
 * Gate 2: Master clock buffer (2A pin 3, 2Y pin 4).
 * Gates 3-6: Auxiliary inverters (3A/3Y, 4A/4Y, 5A/5Y, 6A/6Y).
 * Pin 7: GND, Pin 14: VCC.
 */
typedef struct R01aSn74hcu04 {
    NsEntity base;
    NsLevel osc_phase;
} R01aSn74hcu04;

void r01a_sn74hcu04_init(R01aSn74hcu04 *chip, const char *refdes);
NsEntity *r01a_sn74hcu04_entity(R01aSn74hcu04 *chip);

#endif
