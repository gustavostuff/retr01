#ifndef retr01_SIM_SN74HC573_H
#define retr01_SIM_SN74HC573_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Octal transparent latch -- field SRAM A[7:0] (ALE from MCU-S1).
 * LE high: Q follows D. LE low: Q holds. /OE low enables Q.
 */
typedef struct R01sSn74hc573 {
    R01sEntity base;
    uint8_t latched;
} R01sSn74hc573;

void r01s_sn74hc573_init(R01sSn74hc573 *chip, const char *refdes);
R01sEntity *r01s_sn74hc573_entity(R01sSn74hc573 *chip);
uint8_t r01s_sn74hc573_q(const R01sSn74hc573 *chip);

#endif
