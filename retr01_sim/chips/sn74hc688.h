#ifndef RETR01_SIM_SN74HC688_H
#define RETR01_SIM_SN74HC688_H

#include "retr01_sim/entity.h"

/*
 * 8-bit identity comparator — raster Y vs $FE04 (island H).
 * Pinout abstract: hw/md/SN74HC688.md (P[], Q[], OE#, EQ#).
 */
typedef struct R01sSn74hc688 {
    R01sEntity base;
} R01sSn74hc688;

void r01s_sn74hc688_init(R01sSn74hc688 *chip, const char *refdes);
R01sEntity *r01s_sn74hc688_entity(R01sSn74hc688 *chip);

#endif
