#ifndef RETR01_SIM_SN74HC161_H
#define RETR01_SIM_SN74HC161_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Sync 4-bit binary counter — beam stages (island H discrete path).
 * Pinout: hw/md/SN74HC161.md
 * tick: rising CLK edge (CLK sampled vs prev). CLR# async on eval.
 */
typedef struct R01sSn74hc161 {
    R01sEntity base;
    uint8_t q; /* QA..QD in bits 0..3 */
    R01sLevel clk_prev;
} R01sSn74hc161;

void r01s_sn74hc161_init(R01sSn74hc161 *chip, const char *refdes);
R01sEntity *r01s_sn74hc161_entity(R01sSn74hc161 *chip);
uint8_t r01s_sn74hc161_peek_q(const R01sSn74hc161 *chip);

#endif
