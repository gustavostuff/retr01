#include "sn74hc574.h"
#include "test_common.h"

#include "retr01_sim/bus.h"

int main(void) {
    R01sSn74hc574 chip;
    R01sEntity *e;

    r01s_sn74hc574_init(&chip, "U574");
    e = r01s_sn74hc574_entity(&chip);
    expect_true(e != NULL, "entity");

    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "CLK", R01S_LVL_L);
    r01s_entity_eval(e);
    r01s_bus_write(e, "D", 8, 0x3C);
    r01s_entity_drive(e, "CLK", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_sn74hc574_q(&chip) == 0x3C, "rising edge load");
    r01s_bus_write(e, "D", 8, 0x00);
    r01s_entity_eval(e);
    expect_true(r01s_sn74hc574_q(&chip) == 0x3C, "hold while CLK high");
    r01s_entity_drive(e, "CLK", R01S_LVL_L);
    r01s_entity_eval(e);
    r01s_sn74hc574_force_q(&chip, 0x11);
    expect_true(r01s_sn74hc574_q(&chip) == 0x11, "force_q");

    return test_done("test_sn74hc574");
}
