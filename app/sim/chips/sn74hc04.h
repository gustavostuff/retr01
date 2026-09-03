#ifndef retr01_SIM_SN74HC04_H
#define retr01_SIM_SN74HC04_H

#include "retr01_sim/entity.h"

/* Hex inverter -- hw/md/SN74HC_glue.md (same pinout as HC14). */
typedef struct R01sSn74hc04 {
    R01sEntity base;
} R01sSn74hc04;

void r01s_sn74hc04_init(R01sSn74hc04 *chip, const char *refdes);
R01sEntity *r01s_sn74hc04_entity(R01sSn74hc04 *chip);

#endif
