#ifndef RETR01_SIM_STUB14_H
#define RETR01_SIM_STUB14_H

#include "retr01_sim/entity.h"

/*
 * Placeholder 14-pin DIP — proves chip "inheritance" from R01sEntity.
 * Replace with real parts under chips/ (W65C02S, AS6C62256, ...).
 */
typedef struct R01sStub14 {
    R01sEntity base;
    int eval_count;
} R01sStub14;

void r01s_stub14_init(R01sStub14 *chip, const char *refdes);
R01sEntity *r01s_stub14_entity(R01sStub14 *chip);

#endif
