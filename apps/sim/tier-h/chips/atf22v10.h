#ifndef retr01_SIM_ATF22V10_H
#define retr01_SIM_ATF22V10_H

#include "retr01_sim/bom32.h"
#include "retr01_sim/entity.h"
#include "retr01_sim/timing.h"

#include <stdint.h>

/*
 * ATF22V10 -- behavioral PLD shell (32-IC BOM has five).
 * DECODE: $7Fxx select equations -> SEL_7F* (board qualifies soft / PLD reg loads).
 * VRAM: I->Y passthrough stub until interleave equations land.
 * BEAM_Y: P==Q -> EQ#. Beam-X / compositor / BG fetch are dedicated models.
 * With R01S_PROP_DELAY, SEL / Y / EQ# update after PLD tPD.
 */
typedef struct R01sAtf22v10 {
    R01sEntity base;
    int role; /* R01S_PLD_* when used as generic stub */
    uint8_t p_bus;
    uint8_t q_bus;
    int eq; /* beam-Y raster compare result */
    R01sDelayU8 out_delay; /* decode SEL mask, VRAM Y, or EQ# bit0 */
} R01sAtf22v10;

void r01s_atf22v10_init(R01sAtf22v10 *chip, const char *refdes, int role);
R01sEntity *r01s_atf22v10_entity(R01sAtf22v10 *chip);
/* DIP-24 package pads other than pin 12 GND and pin 24 VCC.
 * Names are the datasheet pin class. They are not a fuse map.
 * Skips a number that already carries a signal. */
void r01s_atf22v10_add_shell_pins(R01sEntity *e);
/* Next free signal pin on a DIP-24. Skips 12 and 24. Returns 0 when full. */
int r01s_atf22v10_alloc_pin(int *next);
int r01s_atf22v10_eq(const R01sAtf22v10 *chip);

#endif
