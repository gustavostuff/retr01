#include "avr128db28_s1.h"

#include "discrete_ic/bus.h"

#include <string.h>

static void s1_reset(NsEntity *e) {
    R01aAvr128db28S1 *c = (R01aAvr128db28S1 *)e;
    c->frame = 0;
    c->alive = 0;
    ns_bus_hiz(e, "AD", 8);
    ns_entity_drive(e, "ALE", NS_LVL_L);
    ns_entity_drive(e, "/WE", NS_LVL_H);
    ns_entity_drive(e, "S1_RDY", NS_LVL_H);
}

static void s1_eval(NsEntity *e) {
    ns_bus_hiz(e, "AD", 8);
    (void)e;
}

static void s1_tick(NsEntity *e) {
    R01aAvr128db28S1 *c = (R01aAvr128db28S1 *)e;
    if (ns_level_is_high(ns_entity_sense(e, "VBL"))) {
        c->alive = 1;
    }
    ns_entity_drive(e, "RUN", c->alive ? NS_LVL_H : NS_LVL_L);
}

static void s1_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable MCU_S1_VT = {s1_reset, s1_eval, s1_tick, s1_destroy};

void r01a_avr128db28_s1_init(R01aAvr128db28S1 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &MCU_S1_VT, "AVR128DB28", refdes ? refdes : "US1");
    chip->base.impl = chip;

    ns_entity_add_pin(&chip->base, 1, "RESET#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "AD0", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 3, "AD1", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 4, "AD2", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 5, "AD3", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 6, "AD4", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 7, "AD5", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 8, "AD6", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 9, "AD7", NS_PIN_IO);
    ns_entity_add_pin(&chip->base, 10, "VCC", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 11, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 12, "A8", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 13, "A9", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 14, "A10", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 15, "A11", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 16, "A12", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 17, "A13", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 18, "A14", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 19, "SPI_MOSI", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 20, "SPI_MISO", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 21, "SPI_SCK", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 22, "/SS_S1", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 23, "ALE", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 24, "/WE", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 25, "S1_RDY", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 26, "VBL", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 27, "RUN", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 28, "UPDI", NS_PIN_IN);
    ns_entity_set_dip_mm(&chip->base, 28, 35, 8);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_avr128db28_s1_entity(R01aAvr128db28S1 *chip) {
    return chip ? &chip->base : NULL;
}

uint32_t r01a_avr128db28_s1_frame(const R01aAvr128db28S1 *chip) {
    return chip ? chip->frame : 0;
}

void r01a_avr128db28_s1_bump_frame(R01aAvr128db28S1 *chip) {
    if (chip) {
        chip->frame++;
        chip->alive = 1;
    }
}
