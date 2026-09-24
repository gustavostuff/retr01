#ifndef retr01_SIM_PRG_ROM_H
#define retr01_SIM_PRG_ROM_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_PRG_ROM_BYTES 32768u

/*
 * Island C -- tiny 32 KB PRG ROM stub (breadboard stand-in for cart PRG).
 * Read-only: CE# + OE# low => DQ = mem[A14..A0].
 * CPU maps this at $8000-$FFFF (A15 decode external).
 */
typedef struct R01sPrgRom {
    R01sEntity base;
    uint8_t mem[R01S_PRG_ROM_BYTES];
} R01sPrgRom;

void r01s_prg_rom_init(R01sPrgRom *chip, const char *refdes);
R01sEntity *r01s_prg_rom_entity(R01sPrgRom *chip);

void r01s_prg_rom_load(R01sPrgRom *chip, uint16_t addr, const uint8_t *data, uint16_t len);
void r01s_prg_rom_set_reset_vec(R01sPrgRom *chip, uint16_t entry);
uint8_t r01s_prg_rom_peek(const R01sPrgRom *chip, uint16_t addr);

#endif
