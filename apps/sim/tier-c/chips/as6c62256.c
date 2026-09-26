#include "as6c62256.h"

#include "discrete_ic/bus.h"

#include <stdlib.h>
#include <string.h>

static uint32_t sram_addr(const NsEntity *e) {
    uint32_t a = ns_bus_read(e, "A", 15) & 0x7FFFu;
    (void)e;
    return a;
}

static void sram_reset(NsEntity *e) {
    R01aAs6c62256 *c = (R01aAs6c62256 *)e;
    if (c->mem) {
        memset(c->mem, 0, R01A_SRAM_SIZE);
    }
    ns_bus_hiz(e, "DQ", 8);
}

static void sram_eval(NsEntity *e) {
    R01aAs6c62256 *c = (R01aAs6c62256 *)e;
    int ce = ns_level_is_low(ns_entity_sense(e, "CE#"));
    int oe = ns_level_is_low(ns_entity_sense(e, "OE#"));
    int we = ns_level_is_low(ns_entity_sense(e, "WE#"));
    uint32_t addr;

    if (!ce) {
        ns_bus_hiz(e, "DQ", 8);
        return;
    }
    addr = sram_addr(e);
    if (!c->mem) {
        ns_bus_hiz(e, "DQ", 8);
        return;
    }
    if (we) {
        c->mem[addr] = (uint8_t)ns_bus_read(e, "DQ", 8);
        ns_bus_hiz(e, "DQ", 8);
        return;
    }
    if (oe) {
        ns_bus_write(e, "DQ", 8, c->mem[addr]);
        return;
    }
    ns_bus_hiz(e, "DQ", 8);
}

static void sram_tick(NsEntity *e) {
    (void)e;
}

static void sram_destroy(NsEntity *e) {
    R01aAs6c62256 *c = (R01aAs6c62256 *)e;
    if (!c || !c->mem) {
        return;
    }
    free(c->mem);
    c->mem = NULL;
}

static const NsEntityVTable SRAM_VT = {sram_reset, sram_eval, sram_tick, sram_destroy};

void r01a_as6c62256_init(R01aAs6c62256 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    chip->mem = (uint8_t *)calloc(1, R01A_SRAM_SIZE);
    ns_entity_init(&chip->base, &SRAM_VT, "AS6C62256", refdes ? refdes : "U41");
    chip->base.impl = chip;
    if (!chip->mem) {
        return;
    }

    ns_entity_add_pin(&chip->base, 1, "A14", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "A12", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 3, "A7", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "A6", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 5, "A5", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 6, "A4", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 7, "A3", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 8, "A2", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 9, "A1", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 10, "A0", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 11, "DQ0", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 12, "DQ1", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 13, "DQ2", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 14, "VSS", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 15, "DQ3", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 16, "DQ4", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 17, "DQ5", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 18, "DQ6", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 19, "DQ7", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 20, "CE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 21, "A10", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 22, "OE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 23, "A11", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 24, "A9", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 25, "A8", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 26, "A13", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 27, "WE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 28, "VCC", NS_PIN_PWR);
    ns_entity_set_dip_mm(&chip->base, 28, 37, 13);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_as6c62256_entity(R01aAs6c62256 *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01a_as6c62256_peek(const R01aAs6c62256 *chip, uint32_t addr) {
    if (!chip || !chip->mem || addr >= R01A_SRAM_SIZE) {
        return 0;
    }
    return chip->mem[addr];
}

void r01a_as6c62256_poke(R01aAs6c62256 *chip, uint32_t addr, uint8_t data) {
    if (!chip || !chip->mem || addr >= R01A_SRAM_SIZE) {
        return;
    }
    chip->mem[addr] = data;
}
