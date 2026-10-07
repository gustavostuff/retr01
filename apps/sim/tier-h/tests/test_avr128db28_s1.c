#include "avr128db28_s1.h"
#include "avr128db28_pins.h"
#include "test_common.h"

#include "retr01_sim/bus.h"

static void expect_pin_pad(R01sEntity *e, const char *name, int pad, const char *msg) {
    const R01sPin *p = r01s_entity_pin_named(e, name);
    expect_true(p != NULL && p->number == pad, msg);
}

int main(void) {
    R01sAvr128db28S1 chip;
    R01sEntity *e;

    r01s_avr128db28_s1_init(&chip, "US1");
    e = r01s_avr128db28_s1_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(r01s_entity_pin_named(e, "AD0") != NULL, "SoT AD0");
    expect_true(r01s_entity_pin_named(e, "/WE") != NULL, "SoT /WE");
    expect_true(r01s_entity_pin_named(e, "CE#") == NULL, "no parallel CE#");
    expect_pin_pad(e, "VDD", R01S_AVR_VDD, "VDD pad 20");
    expect_pin_pad(e, "VDDIO2", R01S_AVR_VDDIO2, "VDDIO2 pad 6");
    expect_pin_pad(e, "AVDD", R01S_AVR_AVDD, "AVDD pad 14");
    expect_pin_pad(e, "GND", R01S_AVR_GND1, "GND pad 15");
    expect_pin_pad(e, "GND2", R01S_AVR_GND2, "GND2 pad 21");
    expect_true(r01s_entity_pin_named(e, "VCC") == NULL, "no VCC alias");

    r01s_avr128db28_s1_oam_poke(&chip, 0, 0x10);
    r01s_avr128db28_s1_oam_poke(&chip, 1, 0x01);
    r01s_avr128db28_s1_oam_poke(&chip, 2, 0x00);
    r01s_avr128db28_s1_oam_poke(&chip, 3, 0x20);
    expect_true(r01s_avr128db28_s1_oam_peek(&chip, 0) == 0x10, "OAM Y");
    expect_true(r01s_avr128db28_s1_oam_peek(&chip, 1) == 0x01, "OAM tile");
    expect_true(r01s_avr128db28_s1_oam_peek(&chip, 3) == 0x20, "OAM X");

    r01s_avr128db28_s1_oam_poke(&chip, 400, 0x5A);
    expect_true(r01s_avr128db28_s1_oam_peek(&chip, 400) == 0x5A, "direct poke high OAM");

    r01s_entity_drive(e, "CLK", R01S_LVL_H);
    r01s_entity_tick(e);
    expect_true(r01s_avr128db28_s1_alive(&chip), "alive");

    return test_done("test_avr128db28_s1");
}
