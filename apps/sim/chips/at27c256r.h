#ifndef retr01_SIM_AT27C256R_H
#define retr01_SIM_AT27C256R_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_COLOR_PROM_ENTRIES 64

/*
 * Color PROM: AT27C256R OTP, 64 kit entries, packed R3G3B2.
 * Video path drives A[5:0]. Unused address pins are tied low on the board.
 * Read-only: CE# and OE# low => O[7:0] = PROM[index]. No CPU data-bus access.
 */
typedef struct R01sAt27c256r {
    R01sEntity base;
    uint8_t mem[R01S_COLOR_PROM_ENTRIES];
} R01sAt27c256r;

void r01s_at27c256r_init(R01sAt27c256r *chip, const char *refdes);
R01sEntity *r01s_at27c256r_entity(R01sAt27c256r *chip);

void r01s_at27c256r_load_kit(R01sAt27c256r *chip);
/* Logical kit RGB (Studio/emu SoT). PROM mem[] stays R3G3B2 for the IC path. */
void r01s_at27c256r_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t r01s_at27c256r_peek(const R01sAt27c256r *chip, int index);
void r01s_at27c256r_unpack_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
