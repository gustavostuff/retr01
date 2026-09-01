#ifndef retr01_SIM_SN74HC157_H
#define retr01_SIM_SN74HC157_H

#include "retr01_sim/entity.h"
#include "retr01_sim/timing.h"

/*
 * Quad 2:1 mux -- VRAM / line-buffer address select (island G / M).
 * Pinout: hw/md/SN74HC157.md
 * Select pin named "AB" (datasheet A/B). Strobe "G#" active-low.
 * With R01S_PROP_DELAY, Y[3:0] update after HC157 tpd.
 */
typedef struct R01sSn74hc157 {
    R01sEntity base;
    R01sDelayU8 y_delay; /* low 4 bits = 1Y..4Y */
} R01sSn74hc157;

void r01s_sn74hc157_init(R01sSn74hc157 *chip, const char *refdes);
R01sEntity *r01s_sn74hc157_entity(R01sSn74hc157 *chip);

#endif
