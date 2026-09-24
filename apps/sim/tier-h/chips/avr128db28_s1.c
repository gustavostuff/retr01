#include "avr128db28_s1.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void s1_reset(R01sEntity *e) {
    R01sAvr128db28S1 *c = (R01sAvr128db28S1 *)e;
    memset(c->oam, 0xFF, sizeof(c->oam));
    c->oam_addr = 0;
    c->clk_ticks = 0;
    c->alive = 0;
    r01s_bus_hiz(e, "AD", 8);
    r01s_entity_drive(e, "ALE", R01S_LVL_L);
    r01s_entity_drive(e, "/WE", R01S_LVL_H);
}

static void s1_eval(R01sEntity *e) {
    (void)e;
    /* OAM apply is mailbox/API only (no parallel CE#/DQ). Field ALE//WE driven by board. */
    r01s_bus_hiz(e, "AD", 8);
}

static void s1_tick(R01sEntity *e) {
    R01sAvr128db28S1 *c = (R01sAvr128db28S1 *)e;
    if (r01s_level_is_high(r01s_entity_sense(e, "CLK"))) {
        c->clk_ticks++;
        c->alive = 1;
    }
    r01s_entity_drive(e, "RUN", c->alive ? R01S_LVL_H : R01S_LVL_L);
}

static void s1_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable MCU_S1_VT = {s1_reset, s1_eval, s1_tick, s1_destroy};

void r01s_avr128db28_s1_init(R01sAvr128db28S1 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &MCU_S1_VT, "AVR128DB28", refdes ? refdes : "US1");
    chip->base.impl = chip;

    /* SoT: AD mux + Ahi + SPI ALT1 + ALE + /WE + S1_RDY (no CPU CE#/DQ). */
    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "AD0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 3, "AD1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 4, "AD2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 5, "AD3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "AD4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 7, "AD5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "AD6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 9, "AD7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 10, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 12, "A8", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "A9", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "A10", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 15, "A11", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "A12", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 17, "A13", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 18, "A14", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 19, "SPI_MOSI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "SPI_MISO", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 21, "SPI_SCK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 22, "/SS_S1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "ALE", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 24, "/WE", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 25, "S1_RDY", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 26, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "RUN", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 28, "UPDI", R01S_PIN_IN);
    r01s_entity_set_dip_mm(&chip->base, 28, 35, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_avr128db28_s1_entity(R01sAvr128db28S1 *chip) {
    return chip ? &chip->base : NULL;
}

uint16_t r01s_avr128db28_s1_oam_addr(const R01sAvr128db28S1 *chip) {
    return chip ? chip->oam_addr : 0;
}

uint8_t r01s_avr128db28_s1_oam_peek(const R01sAvr128db28S1 *chip, uint16_t addr) {
    if (!chip || addr >= R01S_OAM_BYTES) {
        return 0;
    }
    return chip->oam[addr];
}

void r01s_avr128db28_s1_oam_poke(R01sAvr128db28S1 *chip, uint16_t addr, uint8_t data) {
    if (!chip || addr >= R01S_OAM_BYTES) {
        return;
    }
    chip->oam[addr] = data;
}

uint32_t r01s_avr128db28_s1_clk_ticks(const R01sAvr128db28S1 *chip) {
    return chip ? chip->clk_ticks : 0;
}

int r01s_avr128db28_s1_alive(const R01sAvr128db28S1 *chip) {
    return chip && chip->alive;
}
