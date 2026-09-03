#ifndef retr01_SIM_SN74HC595_H
#define retr01_SIM_SN74HC595_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * 8-bit serial-in parallel-out shift register (docs/cart.md flasher address chain).
 * SER -> shift on SRCLK; RCLK latches shift reg -> Q0..Q7; Q7S daisy-chains to next SER.
 */
typedef struct R01sSn74hc595 {
    R01sEntity base;
    uint8_t shift;
    uint8_t latched;
    R01sLevel srclk_prev;
    R01sLevel rclk_prev;
} R01sSn74hc595;

void r01s_sn74hc595_init(R01sSn74hc595 *chip, const char *refdes);
R01sEntity *r01s_sn74hc595_entity(R01sSn74hc595 *chip);

uint8_t r01s_sn74hc595_latched(const R01sSn74hc595 *chip);

#endif
