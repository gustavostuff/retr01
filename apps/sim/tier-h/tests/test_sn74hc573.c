#include "sn74hc573.h"
#include "test_common.h"

#include "retr01_sim/bus.h"

int main(void) {
    R01sSn74hc573 chip;
    R01sEntity *e;

    r01s_sn74hc573_init(&chip, "U573");
    e = r01s_sn74hc573_entity(&chip);
    expect_true(e != NULL, "entity");

    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_bus_write(e, "D", 8, 0xA5);
    r01s_entity_drive(e, "LE", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_sn74hc573_q(&chip) == 0xA5, "transparent load");
    r01s_entity_drive(e, "LE", R01S_LVL_L);
    r01s_entity_eval(e);
    r01s_bus_write(e, "D", 8, 0x00);
    r01s_entity_eval(e);
    expect_true(r01s_sn74hc573_q(&chip) == 0xA5, "hold when LE low");

    return test_done("test_sn74hc573");
}
