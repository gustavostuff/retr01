#ifndef RETR01_SIM_I2C_EEPROM_H
#define RETR01_SIM_I2C_EEPROM_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Cart save EEPROM (32-IC BOM: 24C64-class I2C device on cartridge).
 * Read-only stub — protocol window TBD in docs/02.
 */
typedef struct R01sI2cEeprom {
    R01sEntity base;
    uint8_t mem[256];
} R01sI2cEeprom;

void r01s_i2c_eeprom_init(R01sI2cEeprom *chip, const char *refdes);
R01sEntity *r01s_i2c_eeprom_entity(R01sI2cEeprom *chip);

#endif
