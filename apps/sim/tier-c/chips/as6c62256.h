#ifndef R01A_AS6C62256_H
#define R01A_AS6C62256_H

#include "netlist_sim/entity.h"

#include <stdint.h>

#define R01A_FIELD_W 128
#define R01A_FIELD_H 120
#define R01A_FIELD_BYTES ((size_t)R01A_FIELD_W * (size_t)R01A_FIELD_H)

#define R01A_SRAM_SIZE 32768u

typedef struct R01aAs6c62256 {
    NsEntity base;
    uint8_t *mem;
} R01aAs6c62256;

void r01a_as6c62256_init(R01aAs6c62256 *chip, const char *refdes);
NsEntity *r01a_as6c62256_entity(R01aAs6c62256 *chip);

uint8_t r01a_as6c62256_peek(const R01aAs6c62256 *chip, uint32_t addr);
void r01a_as6c62256_poke(R01aAs6c62256 *chip, uint32_t addr, uint8_t data);

static inline uint8_t r01a_field_kit_at(const R01aAs6c62256 *chip, int x, int y) {
    if (!chip || !chip->mem || x < 0 || x >= R01A_FIELD_W || y < 0 || y >= R01A_FIELD_H) {
        return 0;
    }
    return chip->mem[(size_t)y * (size_t)R01A_FIELD_W + (size_t)x];
}

#endif
