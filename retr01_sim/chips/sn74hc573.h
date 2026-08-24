#ifndef RETR01_SIM_SN74HC573_H
#define RETR01_SIM_SN74HC573_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Octal transparent latch — island D / $FExx holds.
 * Pinout: hw/md/SN74HC573.md
 */
typedef struct R01sSn74hc573 {
    R01sEntity base;
    uint8_t latched; /* Q held when LE low */
} R01sSn74hc573;

void r01s_sn74hc573_init(R01sSn74hc573 *chip, const char *refdes);
R01sEntity *r01s_sn74hc573_entity(R01sSn74hc573 *chip);

uint8_t r01s_sn74hc573_peek_q(const R01sSn74hc573 *chip);
void r01s_sn74hc573_poke_q(R01sSn74hc573 *chip, uint8_t value);

#endif
