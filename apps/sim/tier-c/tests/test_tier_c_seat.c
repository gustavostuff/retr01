#include "r01a_board.h"

#include "avr128db28_pins.h"
#include "discrete_ic/entity.h"
#include "test_common.h"

static void expect_pin_pad(NsEntity *e, const char *name, int pad, const char *msg) {
    const NsPin *p = ns_entity_pin_named(e, name);
    expect_true(p != NULL && p->number == pad, msg);
}

int main(void) {
    R01aBoard board;
    NsEntity *s1;

    r01a_board_init(&board);
    expect_true(board.extra_bb_count == 0, "code starts with BB1, like Tier B");
    s1 = r01a_avr128db28_s1_entity(&board.mcu_s1);
    expect_true(s1 != NULL, "S1 is present");
    expect_pin_pad(s1, "VDD", R01S_AVR_VDD, "S1 VDD pad 20");
    expect_pin_pad(s1, "VDDIO2", R01S_AVR_VDDIO2, "S1 VDDIO2 pad 6");
    expect_pin_pad(s1, "AVDD", R01S_AVR_AVDD, "S1 AVDD pad 14");
    expect_pin_pad(s1, "GND", R01S_AVR_GND1, "S1 GND pad 15");
    expect_pin_pad(s1, "GND2", R01S_AVR_GND2, "S1 GND2 pad 21");
    expect_true(ns_entity_pin_named(s1, "VCC") == NULL, "S1 has no VCC alias");
    expect_true(r01a_as6c62256_entity(&board.field_sram) != NULL, "field SRAM is present");
    expect_true(r01a_sn74hc573_entity(&board.field_latch) != NULL, "field latch is present");
    expect_true(r01a_board_entity_by_refdes(&board, "C8") != NULL, "field SRAM decoupling cap");
    expect_true(r01a_board_entity_by_refdes(&board, "C6") != NULL, "S1 decoupling cap");
    expect_true(r01a_board_entity_by_refdes(&board, "E1") != NULL, "bulk cap");
    r01a_board_shutdown(&board);
    return test_done("test_tier_c_seat");
}
