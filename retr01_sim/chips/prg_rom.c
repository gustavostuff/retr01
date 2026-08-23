#include "prg_rom.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void prg_reset(R01sEntity *e) {
    r01s_bus_hiz(e, "DQ", 8);
}

static void prg_eval(R01sEntity *e) {
    R01sPrgRom *c = (R01sPrgRom *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    uint16_t addr;

    if (!ce || !oe) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }
    addr = (uint16_t)(r01s_bus_read(e, "A", 15) & 0x7FFFu);
    r01s_bus_write(e, "DQ", 8, c->mem[addr]);
}

static void prg_tick(R01sEntity *e) {
    (void)e;
}

static void prg_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable PRG_VT = {prg_reset, prg_eval, prg_tick, prg_destroy};

static const char *const PRG_A_NAMES[15] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6", "A7",
                                           "A8",  "A9",  "A10", "A11", "A12", "A13", "A14"};
static const char *const PRG_DQ_NAMES[8] = {"DQ0", "DQ1", "DQ2", "DQ3", "DQ4", "DQ5", "DQ6", "DQ7"};

void r01s_prg_rom_init(R01sPrgRom *chip, const char *refdes) {
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &PRG_VT, "PRG_ROM", refdes ? refdes : "U?");
    chip->base.impl = chip;
    memset(chip->mem, 0xEA, sizeof(chip->mem));

    for (i = 0; i < 15; i++) {
        r01s_entity_add_pin(&chip->base, i + 1, PRG_A_NAMES[i], R01S_PIN_IN);
    }
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 16 + i, PRG_DQ_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 24, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 25, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 26, "VSS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 28, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 28, 56);
    r01s_prg_rom_set_reset_vec(chip, 0x8000);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_prg_rom_entity(R01sPrgRom *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_prg_rom_load(R01sPrgRom *chip, uint16_t addr, const uint8_t *data, uint16_t len) {
    uint16_t i;
    if (!chip || !data) {
        return;
    }
    for (i = 0; i < len; i++) {
        chip->mem[(addr + i) & 0x7FFFu] = data[i];
    }
}

void r01s_prg_rom_set_reset_vec(R01sPrgRom *chip, uint16_t entry) {
    /* CPU vectors at $FFFC/$FFFD => offset $7FFC/$7FFD in 32KB window. */
    if (!chip) {
        return;
    }
    chip->mem[0x7FFC] = (uint8_t)(entry & 0xFF);
    chip->mem[0x7FFD] = (uint8_t)((entry >> 8) & 0xFF);
}

uint8_t r01s_prg_rom_peek(const R01sPrgRom *chip, uint16_t addr) {
    if (!chip) {
        return 0;
    }
    return chip->mem[addr & 0x7FFFu];
}
