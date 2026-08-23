#include "retr01_sim/bus.h"
#include "sn74hc161.h"
#include "test_common.h"

int main(void) {
    R01sSn74hc161 chip;
    R01sEntity *e;
    int i;

    r01s_sn74hc161_init(&chip, "U40");
    e = r01s_sn74hc161_entity(&chip);
    expect_true(e->pin_count == 16, "16 pins");

    r01s_entity_drive(e, "CLR#", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_sn74hc161_peek_q(&chip) == 0, "async clear");

    r01s_entity_drive(e, "CLR#", R01S_LVL_H);
    r01s_entity_drive(e, "LOAD#", R01S_LVL_L);
    r01s_entity_drive(e, "A", R01S_LVL_H);
    r01s_entity_drive(e, "B", R01S_LVL_L);
    r01s_entity_drive(e, "C", R01S_LVL_H);
    r01s_entity_drive(e, "D", R01S_LVL_L); /* load 5 */
    r01s_entity_drive(e, "ENP", R01S_LVL_H);
    r01s_entity_drive(e, "ENT", R01S_LVL_H);
    r01s_entity_drive(e, "CLK", R01S_LVL_L);
    r01s_entity_tick(e);
    r01s_entity_drive(e, "CLK", R01S_LVL_H);
    r01s_entity_tick(e);
    expect_true(r01s_sn74hc161_peek_q(&chip) == 5, "load 5");

    r01s_entity_drive(e, "LOAD#", R01S_LVL_H);
    for (i = 0; i < 3; i++) {
        r01s_entity_drive(e, "CLK", R01S_LVL_L);
        r01s_entity_tick(e);
        r01s_entity_drive(e, "CLK", R01S_LVL_H);
        r01s_entity_tick(e);
    }
    expect_true(r01s_sn74hc161_peek_q(&chip) == 8, "count to 8");

    /* Fill to 15 and check RCO */
    while (r01s_sn74hc161_peek_q(&chip) != 15) {
        r01s_entity_drive(e, "CLK", R01S_LVL_L);
        r01s_entity_tick(e);
        r01s_entity_drive(e, "CLK", R01S_LVL_H);
        r01s_entity_tick(e);
    }
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "RCO") == R01S_LVL_H, "RCO at 15");

    return test_done("test_sn74hc161");
}
