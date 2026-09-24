#include "attiny85.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sAttiny85 chip;
    R01sEntity *e;
    uint8_t reply = 0;

    r01s_attiny85_init(&chip, "UPAD1", 0x55u);
    e = r01s_attiny85_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(e->dip_pins == 8, "DIP-8");
    expect_true(e->part && e->part[0] == 'A', "part ATtiny85");

    r01s_entity_eval(e);
    expect_true(r01s_level_pulled(r01s_entity_sense(e, "DATA")) == R01S_LVL_H, "OD idle pulled H");

    r01s_attiny85_set_buttons(&chip, 0xA5);
    expect_true(r01s_attiny85_get_buttons(&chip) == 0xA5, "buttons");

    r01s_attiny85_rx_byte(&chip, 0x55u);
    expect_true(r01s_attiny85_take_reply(&chip, &reply) == 1, "reply on 0x55");
    expect_true(reply == 0xA5, "reply byte");
    expect_true(r01s_level_pulled(r01s_entity_sense(e, "DATA")) == R01S_LVL_H, "DATA released after take");

    r01s_attiny85_rx_byte(&chip, 0xAAu);
    expect_true(r01s_attiny85_take_reply(&chip, &reply) == 0, "silent on wrong poll");

    r01s_attiny85_set_buttons(&chip, 0x12);
    r01s_attiny85_rx_byte(&chip, 0x55u);
    expect_true(r01s_attiny85_take_reply(&chip, &reply) == 1, "second poll");
    expect_true(reply == 0x12, "sampled buttons");

    return test_done("test_attiny85");
}
