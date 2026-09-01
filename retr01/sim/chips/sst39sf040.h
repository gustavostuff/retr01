#ifndef retr01_SIM_SST39SF040_H
#define retr01_SIM_SST39SF040_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_FLASH_BYTES (512u * 1024u)

/*
 * 512 KB cart flash -- read-only stub (program/erase later).
 * Pinout: hw/md/SST39SF040.md
 */
typedef struct R01sSst39sf040 {
    R01sEntity base;
    uint8_t mem[R01S_FLASH_BYTES];
} R01sSst39sf040;

void r01s_sst39sf040_init(R01sSst39sf040 *chip, const char *refdes);
R01sEntity *r01s_sst39sf040_entity(R01sSst39sf040 *chip);

void r01s_sst39sf040_load(R01sSst39sf040 *chip, uint32_t addr, const uint8_t *data, uint32_t len);
uint8_t r01s_sst39sf040_peek(const R01sSst39sf040 *chip, uint32_t addr);
void r01s_sst39sf040_poke(R01sSst39sf040 *chip, uint32_t addr, uint8_t data);

#endif
