#include "sst39sf040.h"

#include "retr01_sim/bus.h"

#include <string.h>

static uint32_t flash_addr(R01sEntity *e) {
    static const char *const names[19] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6",  "A7",  "A8",
                                          "A9",  "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17",
                                          "A18"};
    uint32_t addr = 0;
    int i;
    for (i = 0; i < 19; i++) {
        if (r01s_level_is_high(r01s_entity_sense(e, names[i]))) {
            addr |= (1u << i);
        }
    }
    return addr;
}

static void flash_reset(R01sEntity *e) {
    r01s_bus_hiz(e, "DQ", 8);
}

static void flash_eval(R01sEntity *e) {
    R01sSst39sf040 *c = (R01sSst39sf040 *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_low(r01s_entity_sense(e, "WE#"));

    /* Read-only stub: drive when selected for read (CE#·OE#·!WE#). */
    if (!ce || !oe || we) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    r01s_bus_write(e, "DQ", 8, c->mem[flash_addr(e) & (R01S_FLASH_BYTES - 1u)]);
}

static void flash_tick(R01sEntity *e) {
    (void)e;
}

static void flash_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable FLASH_VT = {flash_reset, flash_eval, flash_tick, flash_destroy};

void r01s_sst39sf040_init(R01sSst39sf040 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &FLASH_VT, "SST39SF040", refdes ? refdes : "U?");
    chip->base.impl = chip;
    memset(chip->mem, 0xFF, sizeof(chip->mem));

    r01s_entity_add_pin(&chip->base, 1, "A18", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "A16", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A15", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "A12", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "A6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "A5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "A4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "A3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "A2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "A1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "DQ0", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "DQ1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 15, "DQ2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "VSS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 17, "DQ3", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 18, "DQ4", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 19, "DQ5", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 20, "DQ6", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 21, "DQ7", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 22, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "A10", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 24, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 25, "A11", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 26, "A9", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "A8", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 28, "A13", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 29, "A14", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 30, "A17", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 31, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 32, "VDD", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 32);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sst39sf040_entity(R01sSst39sf040 *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_sst39sf040_load(R01sSst39sf040 *chip, uint32_t addr, const uint8_t *data, uint32_t len) {
    uint32_t i;
    if (!chip || !data) {
        return;
    }
    for (i = 0; i < len; i++) {
        chip->mem[(addr + i) & (R01S_FLASH_BYTES - 1u)] = data[i];
    }
}

uint8_t r01s_sst39sf040_peek(const R01sSst39sf040 *chip, uint32_t addr) {
    if (!chip) {
        return 0xFF;
    }
    return chip->mem[addr & (R01S_FLASH_BYTES - 1u)];
}

void r01s_sst39sf040_poke(R01sSst39sf040 *chip, uint32_t addr, uint8_t data) {
    if (chip) {
        chip->mem[addr & (R01S_FLASH_BYTES - 1u)] = data;
    }
}
