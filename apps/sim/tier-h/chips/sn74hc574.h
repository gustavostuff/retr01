#ifndef retr01_SIM_SN74HC574_H
#define retr01_SIM_SN74HC574_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Octal edge register -- BG1 scroll X ($7F02).
 * Rising CLK loads D->Q. /OE low enables Q. No CE pin (CLK is the load strobe).
 */
typedef struct R01sSn74hc574 {
    R01sEntity base;
    uint8_t q;
    int clk_prev;
} R01sSn74hc574;

void r01s_sn74hc574_init(R01sSn74hc574 *chip, const char *refdes);
R01sEntity *r01s_sn74hc574_entity(R01sSn74hc574 *chip);
uint8_t r01s_sn74hc574_q(const R01sSn74hc574 *chip);
void r01s_sn74hc574_force_q(R01sSn74hc574 *chip, uint8_t v);

#endif
