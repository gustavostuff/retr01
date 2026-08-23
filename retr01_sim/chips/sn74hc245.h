#ifndef RETR01_SIM_SN74HC245_H
#define RETR01_SIM_SN74HC245_H

#include "retr01_sim/entity.h"

/*
 * Octal bus transceiver — CPU / video / cart isolation.
 * Pinout: hw/md/SN74HC245.md
 */
typedef struct R01sSn74hc245 {
    R01sEntity base;
} R01sSn74hc245;

void r01s_sn74hc245_init(R01sSn74hc245 *chip, const char *refdes);
R01sEntity *r01s_sn74hc245_entity(R01sSn74hc245 *chip);

#endif
