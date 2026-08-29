#ifndef retr01_SIM_SN74HC157_H
#define retr01_SIM_SN74HC157_H

#include "retr01_sim/entity.h"

/*
 * Quad 2:1 mux -- VRAM / line-buffer address select (island G / M).
 * Pinout: hw/md/SN74HC157.md
 * Select pin named "AB" (datasheet A/B). Strobe "G#" active-low.
 */
typedef struct R01sSn74hc157 {
    R01sEntity base;
} R01sSn74hc157;

void r01s_sn74hc157_init(R01sSn74hc157 *chip, const char *refdes);
R01sEntity *r01s_sn74hc157_entity(R01sSn74hc157 *chip);

#endif
