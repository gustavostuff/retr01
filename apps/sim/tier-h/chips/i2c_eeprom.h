#ifndef retr01_SIM_I2C_EEPROM_H
#define retr01_SIM_I2C_EEPROM_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Cart save EEPROM (32-IC BOM: 24C64-class I2C device on cartridge).
 * Cart 24C64. Pin numbers follow the datasheet. The model is a pin shell and a memory array.
 */
typedef struct R01sI2cEeprom {
    R01sEntity base;
    uint8_t mem[256];
} R01sI2cEeprom;

void r01s_i2c_eeprom_init(R01sI2cEeprom *chip, const char *refdes);
R01sEntity *r01s_i2c_eeprom_entity(R01sI2cEeprom *chip);

#endif
