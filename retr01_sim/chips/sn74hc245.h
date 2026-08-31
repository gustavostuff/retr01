#ifndef retr01_SIM_SN74HC245_H
#define retr01_SIM_SN74HC245_H

#include "retr01_sim/entity.h"
#include "retr01_sim/timing.h"

/*
 * Octal bus transceiver -- CPU / video / cart isolation.
 * Pinout: hw/md/SN74HC245.md
 * With R01S_PROP_DELAY, driven side updates after HC245 tpd.
 */
typedef struct R01sSn74hc245 {
    R01sEntity base;
    R01sDelayU8 out_delay;
    int driving_b; /* 1 = A->B, 0 = B->A when OE low */
} R01sSn74hc245;

void r01s_sn74hc245_init(R01sSn74hc245 *chip, const char *refdes);
R01sEntity *r01s_sn74hc245_entity(R01sSn74hc245 *chip);

#endif
