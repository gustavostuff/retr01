#include "retr01_sim/bus.h"
#include "sn74hc595.h"
#include "test_common.h"

static void pulse(R01sEntity *e, const char *name) {
    r01s_entity_drive(e, name, R01S_LVL_L);
    r01s_entity_eval(e);
    r01s_entity_drive(e, name, R01S_LVL_H);
    r01s_entity_eval(e);
}

int main(void) {
    R01sSn74hc595 chip;
    R01sEntity *e;

    r01s_sn74hc595_init(&chip, "U595");
    e = r01s_sn74hc595_entity(&chip);
    expect_true(e->dip_pins == 16, "16-pin DIP");

    r01s_entity_drive(e, "SRCLR#", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "SER", R01S_LVL_H);
    pulse(e, "SRCLK");
    r01s_entity_drive(e, "SER", R01S_LVL_L);
    pulse(e, "SRCLK");
    pulse(e, "RCLK");
    expect_true(r01s_sn74hc595_latched(&chip) == 0x02, "shift 1 then 0 -> latched $02");

    return test_done("test_sn74hc595");
}
