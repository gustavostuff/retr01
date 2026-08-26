#ifndef retr01_SIM_SN74HC32_H
#define retr01_SIM_SN74HC32_H

#include "retr01_sim/entity.h"

/* Quad 2-input OR — hw/md/SN74HC_glue.md */
typedef struct R01sSn74hc32 {
    R01sEntity base;
} R01sSn74hc32;

void r01s_sn74hc32_init(R01sSn74hc32 *chip, const char *refdes);
R01sEntity *r01s_sn74hc32_entity(R01sSn74hc32 *chip);

#endif
