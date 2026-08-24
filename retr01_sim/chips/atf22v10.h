#ifndef RETR01_SIM_ATF22V10_H
#define RETR01_SIM_ATF22V10_H

#include "retr01_sim/bom32.h"
#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * ATF22V10 — behavioral PLD shell (32-IC BOM has five).
 * Decode / VRAM glue / beam-Y compare are minimal combinatorial stubs.
 * Beam-X, compositor, and BG fetch remain dedicated models but use part ATF22V10.
 */
typedef struct R01sAtf22v10 {
    R01sEntity base;
    int role; /* R01S_PLD_* when used as generic stub */
    uint8_t p_bus;
    uint8_t q_bus;
    int eq; /* beam-Y raster compare result */
} R01sAtf22v10;

void r01s_atf22v10_init(R01sAtf22v10 *chip, const char *refdes, int role);
R01sEntity *r01s_atf22v10_entity(R01sAtf22v10 *chip);
int r01s_atf22v10_eq(const R01sAtf22v10 *chip);

#endif
