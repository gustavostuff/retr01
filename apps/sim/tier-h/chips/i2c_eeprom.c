#include "i2c_eeprom.h"

#include <string.h>

static void eeprom_reset(R01sEntity *e) {
    R01sI2cEeprom *c = (R01sI2cEeprom *)e;
    memset(c->mem, 0xFF, sizeof(c->mem));
}

static void eeprom_eval(R01sEntity *e) {
    (void)e;
}

static void eeprom_tick(R01sEntity *e) {
    (void)e;
}

static void eeprom_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable I2C_EEPROM_VT = {eeprom_reset, eeprom_eval, eeprom_tick, eeprom_destroy};

void r01s_i2c_eeprom_init(R01sI2cEeprom *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &I2C_EEPROM_VT, "24C64", refdes ? refdes : "U50");
    chip->base.impl = chip;
    /* 8-pin PDIP per hw/md/24C64.md */
    r01s_entity_add_pin(&chip->base, 1, "A0", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 2, "A1", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 3, "A2", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 4, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 5, "SDA", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "SCL", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "WP#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_i2c_eeprom_entity(R01sI2cEeprom *chip) {
    return chip ? &chip->base : NULL;
}
