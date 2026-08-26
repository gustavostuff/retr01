#ifndef retr01_SIM_AT28C16_H
#define retr01_SIM_AT28C16_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_COLOR_PROM_ENTRIES 64

/*
 * Island O — AT28C16 Color PROM (packed R3G3B2, read-only in play).
 * Address = 6-bit master palette index; data = {RRRGGGBB}.
 */
typedef struct R01sAt28c16 {
    R01sEntity base;
    uint8_t mem[R01S_COLOR_PROM_ENTRIES];
} R01sAt28c16;

void r01s_at28c16_init(R01sAt28c16 *chip, const char *refdes);
R01sEntity *r01s_at28c16_entity(R01sAt28c16 *chip);

void r01s_at28c16_load_kit(R01sAt28c16 *chip);
/* Logical kit RGB (Studio/emu SoT). PROM mem[] stays R3G3B2 for the IC path. */
void r01s_at28c16_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t r01s_at28c16_peek(const R01sAt28c16 *chip, int index);
void r01s_at28c16_unpack_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
