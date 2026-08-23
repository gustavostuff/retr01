#include "as6c62256.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void sram_reset(R01sEntity *e) {
    R01sAs6c62256 *c = (R01sAs6c62256 *)e;
    memset(c->mem, 0, sizeof(c->mem));
    r01s_bus_hiz(e, "DQ", 8);
}

static void sram_eval(R01sEntity *e) {
    R01sAs6c62256 *c = (R01sAs6c62256 *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_low(r01s_entity_sense(e, "WE#"));
    uint16_t addr = (uint16_t)(r01s_bus_read(e, "A", 15) & 0x7FFFu);

    if (!ce) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    if (we) {
        c->mem[addr] = (uint8_t)r01s_bus_read(e, "DQ", 8);
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    if (oe) {
        r01s_bus_write(e, "DQ", 8, c->mem[addr]);
        return;
    }
    r01s_bus_hiz(e, "DQ", 8);
}

static void sram_tick(R01sEntity *e) {
    (void)e;
}

static void sram_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable SRAM_VT = {sram_reset, sram_eval, sram_tick, sram_destroy};

void r01s_as6c62256_init(R01sAs6c62256 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &SRAM_VT, "AS6C62256", refdes ? refdes : "U?");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "A14", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "A12", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "A6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "A4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "A3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "A2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "A1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 12, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "DQ2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "VSS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 15, "DQ3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "DQ4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 17, "DQ5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 18, "DQ6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 19, "DQ7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 20, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 21, "A10", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 22, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "A11", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 24, "A9", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 25, "A8", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 26, "A13", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 28, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 28, 56);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_as6c62256_entity(R01sAs6c62256 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_as6c62256_peek(const R01sAs6c62256 *chip, uint16_t addr) {
    if (!chip) {
        return 0;
    }
    return chip->mem[addr & 0x7FFFu];
}

void r01s_as6c62256_poke(R01sAs6c62256 *chip, uint16_t addr, uint8_t data) {
    if (chip) {
        chip->mem[addr & 0x7FFFu] = data;
    }
}
