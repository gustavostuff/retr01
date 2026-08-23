#ifndef RETR01_SIM_SN74HC08_H
#define RETR01_SIM_SN74HC08_H

#include "retr01_sim/entity.h"

/* Quad 2-input AND — hw/md/SN74HC_glue.md */
typedef struct R01sSn74hc08 {
    R01sEntity base;
} R01sSn74hc08;

void r01s_sn74hc08_init(R01sSn74hc08 *chip, const char *refdes);
R01sEntity *r01s_sn74hc08_entity(R01sSn74hc08 *chip);

#endif
