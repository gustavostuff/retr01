#ifndef RETR01_SIM_OSC8M_H
#define RETR01_SIM_OSC8M_H

#include "retr01_sim/entity.h"

/*
 * Island B — 8.000 MHz can oscillator driving CPU PHI2.
 *
 * Pins (DIP-8 half / 4 used):
 *   1 OE#   4 GND   5 PHI2   8 VDD
 * tick: toggles PHI2 when VDD=H and OE#!=L (OE# Z = enabled).
 */
typedef struct R01sOsc8m {
    R01sEntity base;
    int half_cycles;
} R01sOsc8m;

void r01s_osc8m_init(R01sOsc8m *chip, const char *refdes);
R01sEntity *r01s_osc8m_entity(R01sOsc8m *chip);

#endif
