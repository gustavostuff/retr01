#ifndef retr01_SIM_SN74HC14_H
#define retr01_SIM_SN74HC14_H

#include "retr01_sim/entity.h"

/*
 * Island B -- SN74HC14 hex Schmitt inverter (reset / clock cleanup).
 * Pinout: hw/md/SN74HC_glue.md
 */
typedef struct R01sSn74hc14 {
    R01sEntity base;
} R01sSn74hc14;

void r01s_sn74hc14_init(R01sSn74hc14 *chip, const char *refdes);
R01sEntity *r01s_sn74hc14_entity(R01sSn74hc14 *chip);

#endif
