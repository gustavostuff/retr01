#ifndef R01A_SN74HC573_H
#define R01A_SN74HC573_H

#include "netlist_sim/entity.h"

#include <stdint.h>

typedef struct R01aSn74hc573 {
    NsEntity base;
    uint8_t latched;
} R01aSn74hc573;

void r01a_sn74hc573_init(R01aSn74hc573 *chip, const char *refdes);
NsEntity *r01a_sn74hc573_entity(R01aSn74hc573 *chip);
uint8_t r01a_sn74hc573_q(const R01aSn74hc573 *chip);

#endif
