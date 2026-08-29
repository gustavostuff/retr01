#ifndef retr01_SIM_AS6C62256_H
#define retr01_SIM_AS6C62256_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_SRAM_BYTES 32768u

/*
 * AS6C62256 -- 32 KB SRAM (system RAM on C, VRAM on G, sprite line buffer on M).
 * Pinout / truth table: hw/md/AS6C62256.md
 */
typedef struct R01sAs6c62256 {
    R01sEntity base;
    uint8_t mem[R01S_SRAM_BYTES];
} R01sAs6c62256;

void r01s_as6c62256_init(R01sAs6c62256 *chip, const char *refdes);
R01sEntity *r01s_as6c62256_entity(R01sAs6c62256 *chip);

/* Test helpers */
uint8_t r01s_as6c62256_peek(const R01sAs6c62256 *chip, uint16_t addr);
void r01s_as6c62256_poke(R01sAs6c62256 *chip, uint16_t addr, uint8_t data);

#endif
