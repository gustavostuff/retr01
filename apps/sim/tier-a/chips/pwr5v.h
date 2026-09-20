#ifndef R01A_PWR5V_H
#define R01A_PWR5V_H

#include "netlist_sim/entity.h"

/*
 * Board 5 V rail model (not a BOM line).
 * Pins: 1 VIN  2 EN  3 VDD  4 GND
 * VDD = H iff VIN=H and EN is not L (Z counts as enabled).
 */
typedef struct R01aPwr5v {
    NsEntity base;
    int power_ok;
} R01aPwr5v;

void r01a_pwr5v_init(R01aPwr5v *chip, const char *refdes);
NsEntity *r01a_pwr5v_entity(R01aPwr5v *chip);

#endif
