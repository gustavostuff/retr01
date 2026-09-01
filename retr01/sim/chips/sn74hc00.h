#ifndef retr01_SIM_SN74HC00_H
#define retr01_SIM_SN74HC00_H

#include "retr01_sim/entity.h"

/* Quad 2-input NAND -- hw/md/SN74HC_glue.md */
typedef struct R01sSn74hc00 {
    R01sEntity base;
} R01sSn74hc00;

void r01s_sn74hc00_init(R01sSn74hc00 *chip, const char *refdes);
R01sEntity *r01s_sn74hc00_entity(R01sSn74hc00 *chip);

#endif
