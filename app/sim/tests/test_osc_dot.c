#include "osc_dot.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sOscDot osc;
    R01sEntity *e;
    int i;
    int toggles = 0;
    R01sLevel prev;

    r01s_osc_dot_init(&osc, "Y2");
    e = r01s_osc_dot_entity(&osc);

    r01s_entity_drive(e, "VDD", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    prev = r01s_entity_sense(e, "DOT");
    for (i = 0; i < 8; i++) {
        r01s_entity_tick(e);
        if (r01s_entity_sense(e, "DOT") != prev) {
            toggles++;
            prev = r01s_entity_sense(e, "DOT");
        }
    }
    expect_true(toggles >= 7, "DOT toggles each tick");

    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_tick(e);
    expect_true(r01s_entity_sense(e, "DOT") == R01S_LVL_Z, "OE# low -> hi-Z");

    return test_done("test_osc_dot");
}
