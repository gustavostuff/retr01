#include "avr128db28_s1.h"
#include "test_common.h"

#include "retr01_sim/bus.h"

int main(void) {
    R01sAvr128db28S1 chip;
    R01sEntity *e;

    r01s_avr128db28_s1_init(&chip, "US1");
    e = r01s_avr128db28_s1_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(r01s_entity_pin_named(e, "AD0") != NULL, "SoT AD0");
    expect_true(r01s_entity_pin_named(e, "/WE") != NULL, "SoT /WE");
    expect_true(r01s_entity_pin_named(e, "CE#") == NULL, "no parallel CE#");

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
