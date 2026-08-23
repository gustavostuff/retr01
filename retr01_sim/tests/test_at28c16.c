#include "at28c16.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sAt28c16 chip;
    R01sEntity *e;

    r01s_at28c16_init(&chip, "U24");
    e = r01s_at28c16_entity(&chip);
    expect_true(e != NULL, "entity");

    r01s_at28c16_load_kit(&chip);
    expect_true(r01s_at28c16_peek(&chip, 0) == 0x00, "index0 black");
    expect_true(r01s_at28c16_peek(&chip, 48) == 0xFF, "index48 white");
    expect_true(r01s_at28c16_peek(&chip, 2) == 0x20, "index2 kit maroon");

    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_drive(e, "A0", R01S_LVL_L);
    r01s_entity_drive(e, "A1", R01S_LVL_H);
    r01s_entity_drive(e, "A2", R01S_LVL_L);
    r01s_entity_drive(e, "A3", R01S_LVL_L);
    r01s_entity_drive(e, "A4", R01S_LVL_L);
    r01s_entity_drive(e, "A5", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_at28c16_peek(&chip, 2) == 0x20, "peek index2 after addr latch");

    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        r01s_at28c16_unpack_rgb(0xFF, &r, &g, &b);
        expect_true(r == 255 && g == 255 && b == 255, "unpack white");
    }

    return test_done("test_at28c16");
}
