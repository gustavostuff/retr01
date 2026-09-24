#ifndef R01A_AT27C256R_H
#define R01A_AT27C256R_H

#include "netlist_sim/entity.h"

#include <stdint.h>

#define R01A_COLOR_PROM_ENTRIES 64

/*
 * AT27C256R color PROM. 64 kit entries, packed R3G3B2.
 * Video path uses A[5:0]. Unused address pins are inputs (tie L on the board).
 */
typedef struct R01aAt27c256r {
    NsEntity base;
    uint8_t mem[R01A_COLOR_PROM_ENTRIES];
} R01aAt27c256r;

void r01a_at27c256r_init(R01aAt27c256r *chip, const char *refdes);
NsEntity *r01a_at27c256r_entity(R01aAt27c256r *chip);
void r01a_at27c256r_load_kit(R01aAt27c256r *chip);
uint8_t r01a_at27c256r_peek(const R01aAt27c256r *chip, int index);

#endif
